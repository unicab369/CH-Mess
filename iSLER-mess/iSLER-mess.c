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
#define BUTTON_PIN 	PA4
#define INPUT1_PIN 	PA10
#define INPUT2_PIN 	PA5
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


uint8_t frame_info[] = {0xff, 0x10}; // PDU, len, (maybe not?) needed in RX mode

int is_slave_device() {
	return funDigitalRead(INPUT1_PIN);
}

uint32_t counter = 0;

remote_command_t ping_cmd = {
	.command = 0,
	.value1 = 0,
	.value2 = 0
};

uint32_t delay_send = 0;

void handle_receiving_frame(uint32_t time) {
		// now listen for frames on channel 37. When the RF subsystem
	// detects and finalizes one, "rx_ready" in iSLER.h is set true
	Frame_RX(frame_info, 37, PHY_MODE);
	while(!rx_ready);

	// we stepped over !rx_ready so we got a frame
	remote_command_t* cmd = modiSLER_rx_handler();
	if (cmd) {
		blink(1);
		// printf("Receiv Command: %02X\n", cmd->command);

		switch (cmd->command) {
			case 0xBB:
				if (is_slave_device() > 0) {
					Neo_loadCommand(cmd->value1);
					WS2812BDMAStart(NR_LEDS);
				}

				break;
			case 0xF1:
				if (is_slave_device() > 0) {
					ping_cmd.command = 0xF2;
					ping_cmd.value1 = cmd->value1;
					ping_cmd.value2 = cmd->value2;
					delay_send = millis();
				}

				break;
			case 0xF2:
				if (is_slave_device() == 0) {
					// printf("Received value1: %u, value2: %u\n", 
					// 	cmd->value1, cmd->value2);
					printf("time_diff: %d\n", time - cmd->value2);
				}
				break;
		}
	}
}

int main() {
	SystemInit();
    systick_init();			//! required for millis()

	funGpioInitAll();
	funPinMode(LED, GPIO_CFGLR_OUT_2Mhz_PP);
	funPinMode(INPUT1_PIN, GPIO_CFGLR_IN_PUPD);

	RFCoreInit(LL_TX_POWER_0_DBM);
	printf(".~ ch32fun iSLER ~.\n");
	blink(5);

    WS2812BDMAInit();

	uint32_t sec_time = 0;

	// uint32_t cmdValues[] = { 0x61, 0x62, 0x63, 0x64 };
	stepper32_t command_step = {
		.values = Neo_Event_list,
		.size = 5,
		.index = 0
	};

	Button_t button = {
		.pin = BUTTON_PIN,
		.btn_state = BUTTON_IDLE,
		.debounce_time = 0,
		.release_time = 0,
		.press_time = 0
	};

	button_setup(&button);

	while(1) {
		if (ping_cmd.command == 0xF2 && millis() - delay_send > 200) {
			blink(1);
			printf("Sending value1: %u, value2: %u\n", ping_cmd.value1, ping_cmd.value2);
			modiSLER_loadCommand(&dataFrame, &ping_cmd, sizeof(ping_cmd));
			modiSLER_adv_data(&dataFrame);

			ping_cmd.command = 0;
			ping_cmd.value1 = 0;
			ping_cmd.value2 = 0;
		}

		if (is_slave_device() == 0) {
			leds_frame.is_enabled = 1;

			if (systick_handleTimeout(&sec_time, 3000)) {
				blink(1);

				// remote_command_t remote_cmd1 = {
				// 	.command = 0xBB,
				// 	.value1 = command_step.values[command_step.index],
				// 	.value2 = 0xFFFFFFFF
				// };
				// // move to the next value
				// stepper32_tick(&command_step, 1);
				// printf("Sending value: %08X\n", remote_cmd1.value1);

				remote_command_t remote_cmd1 = {
					.command = 0xF1,
					.value1 = counter++,
					.value2 = sec_time
				};
				// printf("[Master] Sending value1: %u, value2: %u\n", 
				// 	remote_cmd1.value1, remote_cmd1.value2);

				modiSLER_loadCommand(&dataFrame, &remote_cmd1, sizeof(remote_cmd1));
				modiSLER_adv_data(&dataFrame);
			}
		}
		
		handle_receiving_frame(millis());
		button_task(&button, button_onChanged);
		// Neo_task();
	}
}
