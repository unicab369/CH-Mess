#include "ch32fun.h"
#include <stdio.h>

// #define I2C_ENABLED
// #define I2C_SLAVE_ENABLED
// #define UART_ENABLED
#define SPI_ENABLED
// #define WS2812_ENABLED

#define SX126X_ENABLED
// #define SX127X_ENABLED

#include "../Mess-libs/modules/fun_optionByte.h"			// 1480 Bytes?
#include "../Mess-libs/modules/systick_irq.h"				// 76 Bytes?
#include "../Mess-libs/modules/fun_button.h"				// 592 Bytes?
#include "../Mess-libs/modules/fun_encoder.h"				// 136 Bytes?
#include "../Mess-libs/pwm/fun_timPWM.h"					// 224 Bytes?
#include "../Mess-libs/modules/fun_joystick.h"				// 244 Bytes?
#include "../Mess-libs/ws2812/fun_spi_ws2812.h"
#include "../Mess-libs/usb/fun_usb.h"

#ifdef I2C_SLAVE_ENABLED
	#include "../Mess-libs/i2c/lib/i2c_slave.h"					// 908 Bytes? + RAM 76 Bytes
#endif

#ifdef UART_ENABLED
	#include "../Mess-libs/modules/fun_uart.h"				// 168 Bytes? + RAM 48 Bytes
#endif

#ifdef I2C_ENABLED
	#include "2_Device/mng_i2c.h"							// 3032 Bytes? + RAM 1370 Bytes
	#include "2_Device/i2c_devices.h"						// 1016 Bytes? + RAM 16 Bytes
#else
	void mngI2c_load_buttonState(uint32_t time, uint8_t state) {}
	void mngI2c_load_encoder(uint32_t time, uint8_t pos, uint8_t dir) {}
	void mngI2c_load_joystick(uint32_t time, uint16_t x, uint16_t y) {}
	void mngI2c_loadCounter(uint32_t counter, uint32_t runTime) {}
	void mngI2c_printBuff_task() {}
#endif

#ifdef SPI_ENABLED
	#include "../Mess-libs/spi/lib/lib_spi.h"
	#include "../Mess-libs/spi/fun_st77352.h"
	// #include "../Mess-libs/sd_card/mod_sdCard.h"
	#include "../Mess-libs/spi/fun_sx127x.h"
	#include "../Mess-libs/spi/fun_sx126x.h"
#endif


void onI2C_SlaveWrite(uint8_t reg, uint8_t length) {
	printf("IM WRITEEN TO\n\r");
}

void onI2C_SlaveRead(uint8_t reg) {
	printf("IM READEN FROM.\n\r");
}

uint32_t sendtok2;

void button_onChanged(Button_Event_e event, uint32_t time) {
	switch (event) {
		case BTN_SINGLECLICK:
			printf("Single Click\n");
			// USB_SEND_FLAG = 1;
			usb_setKey(0x05);
			break;
		case BTN_DOUBLECLICK:
			printf("Double Click\n");
			break;
		case BTN_LONGPRESS:
			printf("Long Press\n"); break;
	}

	mngI2c_load_buttonState(millis(), event);
}

void encoder_onChanged(uint8_t position, uint8_t direction) {
	// printf("pos relative: %d, direction: %d\n", position, direction);
	mngI2c_load_encoder(millis(), position, direction);
}

void joystick_onChanged(uint16_t x, uint16_t y) {
	mngI2c_load_joystick(millis(), x, y);
}

typedef struct {
	uint32_t cycle_count;
	uint32_t counter;
	uint32_t fullCycle_time;
	uint32_t period_1sec;
	uint32_t period_50ms;
	uint32_t period_100ms;
} Session_t;


//# ------------ CH32V003F4P6 ------------
//# 	ENC_A		PD4 - [ 				] - PD3		ENC_B
//# 	**UTX		PD5 - [ 				] - PD2		DC
//# 	**UTR		PD6 - [ 				] - PD1		**SWIO
//# 	**RST 		PD7 - [ 				] - PC7		**MISO
//# 	J_X			PA1 - [ 	V003F4P6	] - PC6		**MOSI
//# 	J_Y			PA2 - [   	TSSOP-20 	] - PC5		**SCK
//# 	**GND-		GND - [ 				] - PC4		CS0
//# 	PWM			PD0 - [ 				] - PC3		RST0 
//# 	**VCC+		VCC - [ 				] - PC2		**SCL
//# 	BTN			PC0 - [ 				] - PC1		**SDA


//# -------------- XL1262 LORA MODULE --------------
//#   	GND
//#   	MISO
//#   	MOSI
//#   	SCK
//#   	CS
//#   	RST
//# 	VCC

//# -------------- E220-900MM LORA MODULE --------------
//#								     RST
//#   	1 - Vcc					15 - SCK
//#   	2 - GND					14 - CS
//#   	3 - RST					13 - MOSI
//#   	4 - nc					12 - MISO
//#   	5 - nc					11 - BUSY


#define BUTTON_PIN 		PC0
#define SPI_DC_PIN		PD2
#define SPI_RST_PIN		PC3
#define LORA_CS_PIN		PC4
#define ST7735_CS_PIN	PD0

volatile uint8_t i2c_registers[32] = {0xaa};

int main() {
	uint8_t toggleValue = 0;
	SystemInit();
	Delay_Ms(1);
	// usb_setup();

	uint16_t bootCnt = fun_optionByte_getValue();
	bootCnt++;
	fun_optionByte_store(bootCnt);
	printf("Boot Count: %d\n", bootCnt);

	systick_init();			//! required for millis()
	funGpioInitAll();

	//# Button: uses PC0
	static Button_t button1 = { .pin = BUTTON_PIN };
	fun_button_setup(&button1);

	//# Hold BUTTON_PIN low to enter slave mode
	uint8_t master_mode = funDigitalRead(BUTTON_PIN);

	#ifdef I2C_ENABLED
		//# I2C1: uses PC1 & PC2
		modI2C_setup(bootCnt);
		i2c_ina219_setup();

		// Enable Low
		if (master_mode == 0) {
			printf("I2C Slave mode\n");
			SetupI2CSlave(0x77, i2c_registers, sizeof(i2c_registers), onI2C_SlaveWrite, onI2C_SlaveRead, false);
		}
	#endif

	if (master_mode == 0) {
		printf("Slave mode\n");
	}
	


	//# UARTX - DMA1_CH4: uses PD5
	const char message[] = "Hello World!\r\n";

	#ifdef UART_ENABLED
		uart_setup();
		dma_uart_setup();
		uart_rx_setup();
	#endif
	
	// //# TIM1: uses PD0(CH1)
	// static TIM_PWM_t pwm_CH1c = {
	// 	.pin = PD0,
	// 	.TIM = TIM1,
	// 	.CCER = TIM_CC1NE
	// };

	// fun_timPWM_init(&pwm_CH1c);
	// fun_timPWM_reload(&pwm_CH1c);

	//# TIM2: uses PD4(CH1) and PD3(CH2)
	// Encoder_t encoder_a = { 0, 0, 0 };
	// fun_encoder_setup(&encoder_a);

	//# ADC - DMA1_CH1: use PA2(CH0) and PA1(CH1)
	fun_joystick_setup();

	#ifdef SPI_ENABLED
		//# uses SCK-PC5, MOSI-PC6, MISO-PC7,
		//# RST-PD3, DC-P
		funPinMode(SPI_DC_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
        funDigitalWrite(SPI_DC_PIN, 1);

		SPI_init(SPI_RST_PIN, SPI_DC_PIN);
		SPI_DMA_init(DMA1_Channel3);

		uint32_t loRa_Frequency = 915E6;

		#ifdef SX126X_ENABLED
			fun_sx126x_init(loRa_Frequency, LORA_CS_PIN);

			// fun_st7335_init(160, 80, ST7735_CS_PIN);
			// fun_st7735_fill_all(ST_PURPLE);
		#elif defined SX127X_ENABLED
			fun_sx127x_init(loRa_Frequency, LORA_CS_PIN);
			fun_sx72xx_setTxPower(17);

			funPinMode(ST7735_CS_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
			funDigitalWrite(ST7735_CS_PIN, 1);

		#elif defined WS2812_ENABLED
			WS2812BDMAInit();
			Neo_loadCommand(NEO_COLOR_CHASE);
		#endif

		// SPI_init2();
		// FRESULT rc;
		// rc = mod_sdCard_write("testfile.txt", "hello world 1111!\n\r");

		// if (rc == 0) {
		// 	Delay_Ms(200);
		// 	rc = mod_sdCard_loadFile("testfile.txt",0);
		// 	printf("read result: %u\n\r", rc);
		// } else {
		// 	printf("write error: %u\n\r", rc);
		// }
	#endif

	uint32_t now = millis();
	Session_t session = { 0, 0, now };

	while(1) {
		now = millis();

		//# prioritize tasks
		fun_button_task(now, &button1, button_onChanged);
		// fun_timPWM_task(now, &pwm_CH1c);

		#ifdef UART_ENABLED
			uart_rx_task();
		#endif
		
		#ifdef SX127X_ENABLED
			int packetSize = fun_sx72xx_parsePacket();
			if (packetSize) {
				char buff[packetSize + 1];
				fun_sx72xx_readPacket(buff);
				buff[packetSize] = 0;
				printf("Receive Packet RSSI %d: '%s'\n\r", fun_sx72xx_getRssi(loRa_Frequency), buff);

				funDigitalWrite(ST7735_CS_PIN, toggleValue);
				toggleValue = !toggleValue;
				printf(toggleValue ? "ON\n" : "OFF\n");
			}
		
		#elif defined SX126X_ENABLED
			u8 memoryIndex;
			int packetSize = fun_sx126x_parsePacket(0x000000, &memoryIndex);

			if (packetSize) {
				char buf[packetSize];
				s16 rssi, snr;
				fun_sx126x_getReceivedMessage(buf, packetSize, memoryIndex, &rssi, &snr);
				printf("\nReceive RSSI %d SNR %d: '%s'\n\r", rssi, snr, buf);
			}

		#elif WS2812_ENABLED
			Neo_task(now);

		#endif
		
		if (now - session.period_1sec > 1000) {
			session.period_1sec = now;
			printf(".");

			#ifdef I2C_ENABLED
				if (i2cMaster_mode) {
					// uint16_t lux;
					// i2c_bh1750_reading(&lux);

					// uint16_t temp, hum;
					// i2c_sht3x_reading(&temp, &hum);

					int16_t shunt, bus, power, current;
					i2c_ina219_reading(&shunt, &bus, &power, &current);

					mngI2c_loadCounter(session.cycle_count, session.fullCycle_time);

					// uint32_t runtime_i2c = SysTick_getRunTime(ssd1306_draw_test);
					// sprintf(str_output, "I2C runtime: %lu us", runtime_i2c);
					// ssd1306_print_str_at(str_output, 0, 0);
				}
			#endif
			
			#ifdef UART_ENABLED
				// dma_uart_tx(message, sizeof(message) - 1);
			#endif

			#ifdef SPI_ENABLED
				uint8_t loRa_message[] = "Hello World 333333";
				// uint32_t runtime_tft = SysTick_getRunTime(fun_st7735_test);
				// printf("ST7735 runtime: %lu us\n", runtime_tft);

				#ifdef SX126X_ENABLED
					fun_sx126x_send(loRa_message, strlen(loRa_message), 0);
				#elif defined SX127X_ENABLED
					fun_sx72xx_send(loRa_message, sizeof(loRa_message));
				#endif
			#endif

			// reset cycle_count
			session.cycle_count = 0;
		}

		else if (now - session.period_100ms > 100) {
			session.period_100ms = now;

			mngI2c_printBuff_task(now);
		}

		else if (now - session.period_50ms > 50) {
			session.period_50ms = now;

			// fun_encoder_task(&encoder_a, encoder_onChanged);
			fun_joystick_task(joystick_onChanged);
		}

		session.cycle_count++;
		session.fullCycle_time = millis() - now;
	}
}


void SetClock(uint32_t u32Clock) {
	uint32_t u32Div = 0;
	uint32_t SystemCoreClock = 48000000;

	if (u32Clock > 24000000)
		SystemCoreClock = 48000000;
	else if (u32Clock > 12000000) {
		SystemCoreClock = 24000000;
		u32Div = RCC_HPRE_DIV1;
	}
	else if (u32Clock > 8000000) {
		SystemCoreClock = 12000000;
		u32Div = RCC_HPRE_DIV2;
	}
	else if (u32Clock > 6000000) {
		SystemCoreClock = 8000000;
		u32Div = RCC_HPRE_DIV3;
	}
	else if (u32Clock > 4800000) {
		SystemCoreClock = 6000000;
		u32Div = RCC_HPRE_DIV4;
	}
	else if (u32Clock > 4000000) {
		SystemCoreClock = 4800000;
		u32Div = RCC_HPRE_DIV5;
	}
	else if (u32Clock > 3428571) {
		SystemCoreClock = 4000000;
		u32Div = RCC_HPRE_DIV6;
	}
	else if (u32Clock >= 3000000) {
		SystemCoreClock = 3428571;
		u32Div = RCC_HPRE_DIV7;
	}
	else if (u32Clock > 1500000) {
		SystemCoreClock = 3000000;
		u32Div = RCC_HPRE_DIV8;
	}
	else if (u32Clock > 750000) {
		SystemCoreClock = 1500000;
		u32Div = RCC_HPRE_DIV16;
	}
	else if (u32Clock > 375000) {
		SystemCoreClock = 750000;
		u32Div = RCC_HPRE_DIV32;
	}
	else if (u32Clock > 187500) {
		SystemCoreClock = 375000;
		u32Div = RCC_HPRE_DIV64;
	}
	else {
		SystemCoreClock = 187500; // slowest setting for now
		u32Div = RCC_HPRE_DIV128;
	}

	switch (SystemCoreClock) {
		case 48000000: // special case - needs PLL
			/* Flash 0 wait state */
			FLASH->ACTLR &= (uint32_t)((uint32_t)~FLASH_ACTLR_LATENCY);
			FLASH->ACTLR |= (uint32_t)FLASH_ACTLR_LATENCY_1;

			/* HCLK = SYSCLK = APB1 */
			RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;

			/* PLL configuration: PLLCLK = HSI * 2 = 48 MHz */
			RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_PLLSRC));
			RCC->CFGR0 |= (uint32_t)(RCC_PLLSRC_HSI_Mul2);

			/* Enable PLL */
			RCC->CTLR |= RCC_PLLON;
			/* Wait till PLL is ready */
			while((RCC->CTLR & RCC_PLLRDY) == 0) { }

			/* Select PLL as system clock source */
			RCC->CFGR0 &= (uint32_t)((uint32_t)~(RCC_SW));
			RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
			/* Wait till PLL is used as system clock source */
			while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08) {}
			break;

		default: // simpler - just use the RC clock with a divider
			/* Flash 0 wait state */
			FLASH->ACTLR &= (uint32_t)((uint32_t)~FLASH_ACTLR_LATENCY);
			FLASH->ACTLR |= (SystemCoreClock >= 24000000) ? (uint32_t)FLASH_ACTLR_LATENCY_1 : (uint32_t)FLASH_ACTLR_LATENCY_0;

			/* HCLK = SYSCLK = APB1 */
			RCC->CFGR0 |= u32Div;
			break;
	} // switch on clock

	// UpdateDelay();
} /* SetClock() */
