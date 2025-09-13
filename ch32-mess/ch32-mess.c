#include "ch32fun.h"
#include <stdio.h>

#define I2C_ENABLED
#define UART_ENABLED
// #define SPI_ENABLED
// #define I2C_SLAVE_ENABLED

#include "../Mess-libs/modules/fun_optionByte.h"			// 1480 Bytes?
#include "../Mess-libs/modules/systick_irq.h"				// 76 Bytes?
#include "../Mess-libs/modules/fun_button.h"				// 592 Bytes?
#include "../Mess-libs/modules/fun_encoder.h"				// 136 Bytes?
#include "../Mess-libs/pwm/fun_timPWM.h"					// 224 Bytes?
#include "../Mess-libs/modules/fun_joystick.h"				// 244 Bytes?
#include "../Mess-libs/ws2812/fun_spi_ws2812.h"
#include "../Mess-libs/usb/fun_usb.h"

#ifdef I2C_SLAVE_ENABLED
	#include "../Mess-libs/i2c/i2c_slave.h"					// 908 Bytes? + RAM 76 Bytes
#endif

#ifdef UART_ENABLED
	#include "../Mess-libs/modules/fun_uart.h"				// 168 Bytes? + RAM 48 Bytes
#endif

#ifdef I2C_ENABLED
	#include "2_Device/mng_i2c.h"							// 3032 Bytes? + RAM 1370 Bytes
#else
	void mngI2c_load_buttonState(uint32_t time, uint8_t state) {}
	void mngI2c_load_encoder(uint32_t time, uint8_t pos, uint8_t dir) {}
	void mngI2c_load_joystick(uint32_t time, uint16_t x, uint16_t y) {}
	void mngI2c_loadCounter(uint32_t counter, uint32_t runTime) {}
	void mngI2c_printBuff_task() {}
#endif

#ifdef SPI_ENABLED
	#include "../Mess-libs/spi/lib_spi.h"
	#include "../Mess-libs/spi/mod_st7735.h"
	#include "../Mess-libs/sd_card/mod_sdCard.h"
#endif

#define BUTTON_PIN 		PC0


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
	uint32_t timeRef_1sec;
	uint32_t timeRef_50ms;
	uint32_t timeRef_100ms;
} Session_t;


//# 	*ENC_A		PD4 - [ 				] - PD3		*ENC_B
//# 	*UTX		PD5 - [ 				] - PD2
//# 	*UTR		PD6 - [ 				] - PD1		*SWIO
//# 	*RST 		PD7 - [ 				] - PC7		*MISO
//# 	*J_X		PA1 - [ 	V003F4P6	] - PC6		*MOSI
//# 	*J_Y		PA2 - [   	TSSOP-20 	] - PC5		*SCK
//# 	GND -		GND - [ 				] - PC4
//# 	*PWM		PD0 - [ 				] - PC3
//# 	VCC +		VCC - [ 				] - PC2		*SCL
//# 	*BTN		PC0 - [ 				] - PC1		*SDA


volatile uint8_t i2c_registers[32] = {0xaa};

int main() {
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
	uint8_t i2cMaster_mode = funDigitalRead(BUTTON_PIN);

	#ifdef I2C_ENABLED
		//# I2C1: uses PC1 & PC2
		modI2C_setup(bootCnt);
	#endif

	#ifdef I2C_SLAVE_ENABLED
		// Enable Low
		if (i2cMaster_mode == 0) {
			printf("I2C Slave mode\n");
			SetupI2CSlave(0x77, i2c_registers, sizeof(i2c_registers), onI2C_SlaveWrite, onI2C_SlaveRead, false);
		}
	#endif

	//# UARTX - DMA1_CH4: uses PD5
	const char message[] = "Hello World!\r\n";

	#ifdef UART_ENABLED
		uart_setup();
		dma_uart_setup();
		uart_rx_setup();
	#endif
	
	//# TIM1: uses PD0(CH1)
	static TIM_PWM_t pwm_CH1c = {
		.pin = PD0,
		.TIM = TIM1,
		.CCER = TIM_CC1NE
	};

	fun_timPWM_init(&pwm_CH1c);
	fun_timPWM_reload(&pwm_CH1c);

	//# TIM2: uses PD4(CH1) and PD3(CH2)
	Encoder_t encoder_a = { 0, 0, 0 };
	fun_encoder_setup(&encoder_a);

	//# ADC - DMA1_CH1: use PA2(CH0) and PA1(CH1)
	fun_joystick_setup();

	#ifdef SPI_ENABLED
		//# uses SCK-PC5, MOSI-PC6, MISO-PC7,
		//# RST-PD2, DC-PC4
		SPI_init();
		mod_st7735_setup(PC3, PC4);
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

	WS2812BDMAInit();
	Neo_loadCommand(NEO_COLOR_CHASE);

	uint32_t now = millis();
	Session_t session = { 0, 0, now };

	while(1) {
		now = millis();
		session.cycle_count++;

		//# prioritize tasks
		fun_button_task(now, &button1, button_onChanged);
		fun_timPWM_task(now, &pwm_CH1c);
		// uart_rx_task();
		Neo_task(now);
		
		if (now - session.timeRef_1sec > 1000) {
			session.timeRef_1sec = now;

			if (i2cMaster_mode) {
				mngI2c_loadCounter(session.cycle_count, session.fullCycle_time);
			}
			session.cycle_count = 0;
			
			#ifdef UART_ENABLED
				// dma_uart_tx(message, sizeof(message) - 1);
			#endif

			// uint32_t runtime_i2c = SysTick_getRunTime(ssd1306_draw_test);
			// sprintf(str_output, "I2C runtime: %lu us", runtime_i2c);
			// ssd1306_print_str_at(str_output, 0, 0);

			#ifdef SPI_ENABLED
				tft_set_cursor(0, 0);
				tft_print("Hello World!");
				// uint32_t runtime_tft = SysTick_getRunTime(mod_st7735_test2);
				// printf("ST7735 runtime: %lu us\n", runtime_tft);
			#endif
		}

		else if (now - session.timeRef_100ms > 100) {
			session.timeRef_100ms = now;

			mngI2c_printBuff_task(now);
		}

		else if (now - session.timeRef_50ms > 50) {
			session.timeRef_50ms = now;

			fun_encoder_task(&encoder_a, encoder_onChanged);
			fun_joystick_task(joystick_onChanged);
		}

		session.fullCycle_time = millis() - now;
	}
}

