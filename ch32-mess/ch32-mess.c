#include "ch32fun.h"
#include <stdio.h>

#include "1_Foundation/fun_button.h"
#include "1_Foundation/fun_uart.h"
#include "1_Foundation/i2c_slave.h"
#include "1_Foundation/modPWM.h"
#include "1_Foundation/modEncoder.h"
#include "1_Foundation/modJoystick.h"
#include "Display/modST77xx.h"
#include "2_Device/fun_ws2812.h"


#include "2_Device/mng_i2c.h"

#include "ST7735/modTFT.h"
#include "Storage/modStorage.h"

int main()
{
	static const char message[] = "Hello World!\r\n";
	uint32_t counter = 0;
	uint32_t ledc_time = 0;
	uint32_t sec_time = 0;
	uint32_t time_ref = 0;

	M_Encoder encoder_a = {0, 0, 0};
	M_Button button_a = {0xC0, BUTTON_IDLE, 0, 0, 0, 0, 0, 0};

	SystemInit();
	systick_init();			//! required for millis()
	Delay_Ms(100);


	// TIM2 Ch1, Ch2 : uses PD3, PD4.
	modEncoder_setup(&encoder_a);

	// I2C1: uses PC1 & PC2
	modI2C_setup();

	// uses SCK-PC5, MOSI-PC6, RST-PD2, CD-PC4
	// SPI_init();
	// modST7735_setup(PD2, PC4);

	while(1)
	{
		uint32_t now = millis();

		// button_run(&button_a, button_onChanged);
		// modEncoder_task(now, &encoder_a, encoder_onChanged);

		if (now - sec_time > 2000) {
			sec_time = now;

			// modI2C_task2(counter++);

			// // modJoystick_task();
			// // dma_uart_tx(message, sizeof(message) - 1);

			uint32_t runtime_i2c = get_runTime(ssd1306_draw_test);
			sprintf(str_output, "I2C runtime: %lu us", runtime_i2c);
			// ssd1306_print_str_at(str_output, 0, 0);

			// uint32_t runtime_tft = get_runTime(tft_test);
			// printf("ST7735 runtime: %lu us\n", runtime_tft);

			// storage_test();
		}
	}
}
