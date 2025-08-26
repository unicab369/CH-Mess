#include "ch32fun.h"
#include <stdio.h>

#include "../modI2C.h"

int main() {
	static const char message[] = "Hello World!\r\n";
	uint32_t counter = 0;
	uint32_t ledc_time = 0;
	uint32_t sec_time = 0;
	uint32_t time_ref = 0;

	SystemInit();
	// systick_init();			//! required for millis()
	Delay_Ms(100);

	// I2C1: uses PC1 & PC2
	modI2C_setup();

	while(1)
	{
		// uint32_t now = millis();

		// // button_run(&button_a, button_onChanged);
		// // modEncoder_task(now, &encoder_a, encoder_onChanged);

		// if (now - sec_time > 2000) {
		// 	sec_time = now;

		// 	modI2C_task2(counter++);

		// 	// uint32_t runtime_i2c = get_runTime(modI2C_task);
		// 	// sprintf(str_output, "I2C runtime: %lu us", runtime_i2c);
		// 	// ssd1306_print_str_at(str_output, 0, 0);

		// 	// uint32_t runtime_tft = get_runTime(tft_test);
		// 	// printf("ST7735 runtime: %lu us\n", runtime_tft);
		// }
	}
}
