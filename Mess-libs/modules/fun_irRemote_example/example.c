#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#include "../systick_irq.h"
#include "../fun_irRemote.h"
#include "../../fun_log.h"

#define CONFIG_DEBUG_ENABLE_LOGS 0
#define IR_PIN			PC3

void on_irRemote_NecHandler(u16 address, u16 command) {
	printf("Nec: %04X %04X\n", address, command);
}

int main() {
	SystemInit();
	Delay_Ms(100);

	systick_init();			//! required for millis()
	funGpioInitAll();

	int i = 0;
	fun_irRemote_init(IR_PIN);

	LOG_Init(&systick_millis);
	LOG_listUsages();
	LOG_banner("IR Remote Example");


	while(1) {
		fun_irRemote_task(on_irRemote_NecHandler);
		// printf(".");
	}
}
