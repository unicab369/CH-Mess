#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#include "../systick_irq.h"
#include "../fun_irRemote.h"

#define IR_PIN			PC3

int main() {
	SystemInit();
	Delay_Ms(100);

	systick_init();			//! required for millis()
	funGpioInitAll();

	fun_irRemote_init(IR_PIN);

	while(1) {
		fun_irRemote_task();
		// printf(".");
	}
}
