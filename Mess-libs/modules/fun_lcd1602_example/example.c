#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#include "../fun_lcd1602.h"

#define LCD_EN_PIN			PC0
#define LCD_RS_PIN			PD6

int main() {
	SystemInit();
	Delay_Ms(100);
	// systick_init();			//! required for millis()

	funGpioInitAll();

	int pins[] = { -1, -1, -1, -1, 
					PD4, PD5, PA1, PA2 };
						
	u8 mode8bit = fun_lcd1602_loadGPIOs(pins);
	fun_lcd1602_init(LCD_EN_PIN, LCD_RS_PIN);

	// fun_lcd1602_setRow(0);
	fun_lcd1602_printStr("Hello World 111!", 0);
	fun_lcd1602_printStr("Hello World 222", 1);

	while(1) {
		Delay_Ms(1000);
		printf(".");
	}
}
