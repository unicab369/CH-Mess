#include "ch32fun.h"
#include <stdio.h>

#include "modules/util_stepper.h"
#include "modules/util_print.h"
#include "modules/systick_irq.h"
#include "modules/modWS2812.h"
#include "modules/modiSLER.h"
#include "modules/fun_button.h"

#ifdef CH570_CH572
#define LED PA9
#else
#define LED PA8
#endif

MESS_DataFrame_t dataFrame = {
	.preamble = 0xA1A2,
	.control_bits = 0xB1B2,
	.msgCode = 0xC1C2,
	.dest = {0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6},
	.group_id = 0x55,
};

void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms(33);
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if(i) Delay_Ms(33);
	}
}

void button_onChanged(Button_Event_e event, uint32_t time) {
	remote_command_t button_cmd = {
		.command = 0xAA,
		.value1 = event
	};

	switch (event) {
		case BTN_SINGLECLICK:
			printf("Single Click\n");
			modiSLER_loadCommand(&dataFrame, &button_cmd, sizeof(button_cmd));
			modiSLER_adv_data(&dataFrame);
			break;
		case BTN_DOUBLECLICK:
			printf("Double Click\n");
			modiSLER_loadCommand(&dataFrame, &button_cmd, sizeof(button_cmd));
			modiSLER_adv_data(&dataFrame);
			break;
		case BTN_LONGPRESS:
			printf("Long Press\n"); break;
	}
}

int main() {
	SystemInit();
    systick_init();			//! required for millis()

	funGpioInitAll();
	funPinMode(LED, GPIO_CFGLR_OUT_2Mhz_PP);
	funPinMode(PA10, GPIO_CFGLR_IN_PUPD);

	RFCoreInit(LL_TX_POWER_0_DBM);
	printf(".~ ch32fun iSLER ~.\n");
	blink(5);

    WS2812BDMAInit();

	uint32_t sec_time = 0;
	uint8_t frame_info[] = {0xff, 0x10}; // PDU, len, (maybe not?) needed in RX mode

	// uint32_t cmdValues[] = { 0x61, 0x62, 0x63, 0x64 };
	stepper32_t command_step = {
		.values = Neo_Event_list,
		.size = 5,
		.index = 0
	};

	Button_t button = {
		.pin = PA4,
		.btn_state = BUTTON_IDLE,
		.debounce_time = 0,
		.release_time = 0,
		.press_time = 0
	};

	button_setup(&button);

	while(1) {		
		if (funDigitalRead(PA10) == FUN_LOW) {
			move_leds.is_enabled = 1;

			if (systick_handleTimeout(&sec_time, 6000)) {
				remote_command_t remote_cmd1 = {
					.command = 0xBB,
					.value1 = command_step.values[command_step.index],
					.value2 = 0xFFFFFFFF
				};
				// move to the next value
				stepper32_tick(&command_step, 1);

				printf("Sending value: %08X\n", remote_cmd1.value1);

				modiSLER_loadCommand(&dataFrame, &remote_cmd1, sizeof(remote_cmd1));
				modiSLER_adv_data(&dataFrame);
				blink(1);
			}
		} else {
			// now listen for frames on channel 37. When the RF subsystem
			// detects and finalizes one, "rx_ready" in iSLER.h is set true
			Frame_RX(frame_info, 37, PHY_MODE);
			while(!rx_ready);

			// we stepped over !rx_ready so we got a frame
			uint8_t cmd_code = modiSLER_rx_handler();
			if (cmd_code > 0) {
				Neo_loadCommand(cmd_code);
				blink(1);
			}
			
			// if (rx_ready) {
			// 	if (modiSLER_rx_handler()) {
			// 		blink(1);
			// 	}
			// }
		}

        Neo_task();
		button_task(&button, button_onChanged);
	}
}
