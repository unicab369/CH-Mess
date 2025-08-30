#include "ch32fun.h"
#include <stdio.h>

#define BUTTON_PIN 		PD0

#include "1_Foundation/fun_uart.h"
#include "1_Foundation/modPWM.h"
#include "1_Foundation/modEncoder.h"
#include "1_Foundation/modJoystick.h"
#include "Display/modST77xx.h"
#include "2_Device/fun_ws2812.h"

#include "2_Device/mng_i2c.h"

// #include "ST7735/lib_tft.h"
#include "ST7735/modST7735.h"
#include "Storage/modStorage.h"

#include "../Mess-libs/modules/systick_irq.h"
#include "../Mess-libs/modules/fun_button.h"
#include "../Mess-libs/i2c/i2c_slave.h"

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

int main()
{
	static const char message[] = "Hello World!\r\n";
	uint32_t counter = 0;
	uint32_t ledc_time = 0;
	uint32_t sec_time = 0;
	uint32_t time_ref = 0;

	M_Encoder encoder_a = {0, 0, 0};

	SystemInit();
	systick_init();			//! required for millis()

	funGpioInitAll();
	Delay_Ms(100);

	Button_t button1 = { BUTTON_PIN, BUTTON_IDLE, 0, 0, 0, 0, 0, 0 };
	button_setup(&button1);

	// I2C1: uses PC1 & PC2
	modI2C_setup();

	uint8_t slave_mode = funDigitalRead(BUTTON_PIN);
	if (slave_mode == 0) {
		printf("I2C Slave mode\n");
		SetupI2CSlave(0x77, i2c_registers, sizeof(i2c_registers), onI2C_SlaveWrite, onI2C_SlaveRead, false);
	}
	
	// uses SCK-PC5, MOSI-PC6, RST-PD2, DC-PC4
	SPI_init();
	modST7735_setup(PC0, PC3);

	// TIM2 Ch1, Ch2 : uses PD3, PD4.
	// modEncoder_setup(&encoder_a);

	while(1) {
		uint32_t now = millis();

		button_run(&button1, button_onChanged);
		// modEncoder_task(now, &encoder_a, encoder_onChanged);

		if (now - sec_time > 1000) {
			sec_time = now;

			if (slave_mode != 0) {
				modI2C_task(counter++);
			}
			
			// // modJoystick_task();
			// // dma_uart_tx(message, sizeof(message) - 1);

			// uint32_t runtime_i2c = SysTick_getRunTime(ssd1306_draw_test);
			// sprintf(str_output, "I2C runtime: %lu us", runtime_i2c);
			// ssd1306_print_str_at(str_output, 0, 0);

			uint32_t runtime_tft = SysTick_getRunTime(ST7735_test2);
			printf("ST7735 runtime: %lu us\n", runtime_tft);

			// storage_test();
		}
	}
}
