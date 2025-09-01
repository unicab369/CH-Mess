#include "ch32fun.h"
#include <stdio.h>


#include "2_Device/fun_ws2812.h"
#include "2_Device/mng_i2c.h"

#include "../Mess-libs/modules/systick_irq.h"
#include "../Mess-libs/modules/fun_joystick.h"
#include "../Mess-libs/modules/fun_button.h"
#include "../Mess-libs/modules/fun_uart.h"
#include "../Mess-libs/modules/fun_encoder.h"
#include "../Mess-libs/modules/fun_optionByte.h"
#include "../Mess-libs/i2c/i2c_slave.h"
#include "../Mess-libs/pwm/fun_timPWM.h"

#include "../Mess-libs/spi/lib_spi.h"
#include "../Mess-libs/spi/mod_st7735.h"
#include "../Mess-libs/sd_card/mod_sdCard.h"

#define BUTTON_PIN 		PC0

void onI2C_SlaveWrite(uint8_t reg, uint8_t length) {
	printf("IM WRITEEN TO\n\r");
}

void onI2C_SlaveRead(uint8_t reg) {
	printf("IM READEN FROM.\n\r");
}

volatile uint8_t i2c_registers[32] = {0xaa};

void button_onChanged(Button_Event_e event, uint32_t time) {
	switch (event) {
		case BTN_SINGLECLICK:
			printf("Single Click\n");
			break;
		case BTN_DOUBLECLICK:
			printf("Double Click\n");
			break;
		case BTN_LONGPRESS:
			printf("Long Press\n"); break;
	}
}

void encoder_onChanged(Encoder_t *model) {
	printf("pos relative: %d\n", model->relative_pos);
}

typedef struct {
	uint32_t cycle_count;
	uint32_t counter;
	uint32_t timeRef;
} Session_t;


//# 	*ENC_A*		PD4 - [ 				] - PD3		*ENC_B*
//# 	*UTX*		PD5 - [ 				] - PD2
//# 				PD6 - [ 				] - PD1		*SWIO*
//# 	*RST* 		PD7 - [ 				] - PC7		*MISO*
//# 	*J_X*		PA1 - [ 	V003F4P6	] - PC6		*MOSI*
//# 	*J_Y*		PA2 - [   	TSSOP-20 	] - PC5		*SCK*
//# 		x		Vcc - [ 				] - PC4
//# 				PD0 - [ 				] - PC3
//# 		x		GND - [ 				] - PC2		*SCL*
//# 	*BTN*		PC0 - [ 				] - PC1		*SDA*


int main() {
	SystemInit();

	uint16_t bootCnt = fun_optionByte_getValue();
	bootCnt++;
	fun_optionByte_store(bootCnt);
	printf("Boot Count: %d\n", bootCnt);

	systick_init();			//! required for millis()
	funGpioInitAll();
	Delay_Ms(10);
	
	//# Button: uses PC0
	Button_t button1 = { .pin = BUTTON_PIN };
	button_setup(&button1);

	//# I2C1: uses PC1 & PC2
	modI2C_setup();

	//# Hold BUTTON_PIN low to enter slave mode
	uint8_t slave_mode = funDigitalRead(BUTTON_PIN);

	if (slave_mode == 0) {
		printf("I2C Slave mode\n");
		SetupI2CSlave(0x77, i2c_registers, sizeof(i2c_registers), onI2C_SlaveWrite, onI2C_SlaveRead, false);
	}

	//# UARTX - DMA1_CH4: uses PD5
	static const char message[] = "Hello World!\r\n";
	uart_setup();
	dma_uart_setup();

	//# uses SCK-PC5, MOSI-PC6, MISO-PC7,
	//# RST-PD2, DC-PC4
	// SPI_init();
	// mod_st7735_setup(PC0, PC3);
	SPI_init2();

	FRESULT rc;
	rc = mod_sdCard_write("testfile.txt", "hello world 1111!\n\r");

	if (rc == 0) {
		Delay_Ms(200);
		rc = mod_sdCard_loadFile("testfile.txt",0);
		printf("read result: %u\n\r", rc);
	} else {
		printf("write error: %u\n\r", rc);
	}
	
	//# TIM1: uses PD0(CH1)
	TIM_PWM_t pwm_CH1c = {
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

	Session_t session = { 0, 0, millis() };

	while(1) {
		uint32_t now = millis();
		session.cycle_count++;

		button_run(&button1, button_onChanged);
		fun_timPWM_task(now, &pwm_CH1c);
		fun_encoder_task(now, &encoder_a, encoder_onChanged);

		if (now - session.timeRef > 1000) {
			session.timeRef = now;

			if (slave_mode != 0) {
				modI2C_task(session.cycle_count);
			}
			session.cycle_count = 0;
			
			// fun_joystick_task();
			dma_uart_tx(message, sizeof(message) - 1);

			// uint32_t runtime_i2c = SysTick_getRunTime(ssd1306_draw_test);
			// sprintf(str_output, "I2C runtime: %lu us", runtime_i2c);
			// ssd1306_print_str_at(str_output, 0, 0);

			// uint32_t runtime_tft = SysTick_getRunTime(mod_st7735_test2);
			// printf("ST7735 runtime: %lu us\n", runtime_tft);
		}
	}
}
