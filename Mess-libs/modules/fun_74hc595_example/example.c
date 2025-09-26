#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#include "../fun_74hc595.h"

#define HC595_LATCH_PIN 			PC3
#define HC595_CLK_PIN 				PC5
#define HC595_DATA_PIN 				PC6

int main() {
	SystemInit();
	Delay_Ms(100);
    // systick_init();			//! required for millis()

	funGpioInitAll();
	SPI_init(-1, -1);

	hc595_init(HC595_LATCH_PIN);
	// hc595_setup_clkDataPins(HC595_CLK_PIN, HC595_DATA_PIN);

	u16 frame = 10000;

	char val = " ";
	printf("value: %d\n", val);

	while(1) {
		for (int i = 32; i < 126; i++) {
			while(frame-- > 0) {
				hc595_printChar(i);
			}
			frame = 10000;
		}

		

		// Delay_Ms(100);
		// for (int i = 0; i < 8; i++) {
		// 	hc595_setColumn_ON(i);
		// 	Delay_Ms(50);
		// }

		// for (int i = 0; i < 8; i++) {
		// 	hc595_setRow_ON(i);
		// 	Delay_Ms(50);
		// }

		// for (int i = 0; i < 8; i++) {
		// 	for (int j = 0; j < 8; j++) {
		// 		hc595_setPosition_ON(i, j);
		// 		Delay_Ms(30);
		// 	}
		// }

		// hc595_setAll_OFF();
		// Delay_Ms(100);
		// printf(".");
	}
}
