#include "ch32fun.h"
#include <stdio.h>

#include "iSLER.h"

#define PHY_MODE       PHY_1M
#define ACCESS_ADDRESS 0x8E89BED6 // the "BED6" address for BLE advertisements

#include "modules/util_stepper.h"
#include "modules/util_print.h"
#include "modules/systick_irq.h"
#include "modules/modWS2812.h"
// #include "modules/modiSLER.h"
#include "modules/fun_button.h"

#ifdef CH570_CH572
#define LED PA9
#else
#define LED PA8
#define BUTTON_PIN 	PA4
#define INPUT1_PIN 	PA10
#define INPUT2_PIN 	PA5
#endif

// MESS_DataFrame_t dataFrame = {
// 	.preamble = 0xA1A2,
// 	.control_bits = 0xB1B2,
// 	.msgCode = 0xC1C2,
// 	.dest = {0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6},
// 	.group_id = 0x55,
// };

void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms(33);
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if(i) Delay_Ms(33);
	}
}

// void button_onChanged(Button_Event_e event, uint32_t time) {
// 	remote_command_t button_cmd = {
// 		.command = 0xAA,
// 		.value1 = event
// 	};

// 	switch (event) {
// 		case BTN_SINGLECLICK:
// 			printf("Single Click\n");
// 			modiSLER_loadCommand(&dataFrame, &button_cmd, sizeof(button_cmd));
// 			modiSLER_adv_data(&dataFrame);
// 			break;
// 		case BTN_DOUBLECLICK:
// 			printf("Double Click\n");
// 			modiSLER_loadCommand(&dataFrame, &button_cmd, sizeof(button_cmd));
// 			modiSLER_adv_data(&dataFrame);
// 			break;
// 		case BTN_LONGPRESS:
// 			printf("Long Press\n"); break;
// 	}
// }


// uint8_t frame_info[] = {0xff, 0x10}; // PDU, len, (maybe not?) needed in RX mode

// int is_slave_device() {
// 	return funDigitalRead(INPUT1_PIN);
// }

// uint32_t counter = 0;

// remote_command_t ping_cmd = {
// 	.command = 0,
// 	.value1 = 0,
// 	.value2 = 0
// };

// uint32_t delay_send = 0;

// void handle_receiving_frame(uint32_t time) {
// 	// now listen for frames on channel 37. When the RF subsystem
// 	// detects and finalizes one, "rx_ready" in iSLER.h is set true
// 	// Frame_RX(frame_info, 37, PHY_MODE);
// 	iSLERRX(ACCESS_ADDRESS, 37, PHY_MODE);
// 	while(!rx_ready);

// 	// we stepped over !rx_ready so we got a frame
// 	remote_command_t* cmd = modiSLER_rx_handler();
// 	if (cmd) {
// 		blink(1);
// 		// printf("Receiv Command: %02X\n", cmd->command);

// 		switch (cmd->command) {
// 			case 0xBB:
// 				if (is_slave_device() > 0) {
// 					Neo_loadCommand(cmd->value1);
// 					WS2812BDMAStart(NR_LEDS);
// 				}

// 				break;
// 			case 0xF1:
// 				if (is_slave_device() > 0) {
// 					ping_cmd.command = 0xF2;
// 					ping_cmd.value1 = cmd->value1;
// 					ping_cmd.value2 = cmd->value2;
// 					delay_send = millis();
// 				}

// 				break;
// 			case 0xF2:
// 				if (is_slave_device() == 0) {
// 					// printf("Received value1: %u, value2: %u\n", 
// 					// 	cmd->value1, cmd->value2);
// 					printf("time_diff: %d\n", time - cmd->value2);
// 				}
// 				break;
// 		}
// 	}
// }

// int main2() {
// 	SystemInit();
//     systick_init();			//! required for millis()

// 	funGpioInitAll();
// 	funPinMode(LED, GPIO_CFGLR_OUT_2Mhz_PP);
// 	funPinMode(INPUT1_PIN, GPIO_CFGLR_IN_PUPD);

// 	RFCoreInit(LL_TX_POWER_0_DBM);
// 	printf(".~ ch32fun iSLER ~.\n");
// 	blink(5);

//     WS2812BDMAInit();

// 	uint32_t sec_time = 0;

// 	// uint32_t cmdValues[] = { 0x61, 0x62, 0x63, 0x64 };
// 	stepper32_t command_step = {
// 		.values = Neo_Event_list,
// 		.size = 5,
// 		.index = 0
// 	};

// 	Button_t button = {
// 		.pin = BUTTON_PIN,
// 		.btn_state = BUTTON_IDLE,
// 		.debounce_time = 0,
// 		.release_time = 0,
// 		.press_time = 0
// 	};

// 	button_setup(&button);

// 	while(1) {
// 		if (ping_cmd.command == 0xF2 && millis() - delay_send > 200) {
// 			blink(1);
// 			printf("Sending value1: %u, value2: %u\n", ping_cmd.value1, ping_cmd.value2);
// 			modiSLER_loadCommand(&dataFrame, &ping_cmd, sizeof(ping_cmd));
// 			modiSLER_adv_data(&dataFrame);

// 			ping_cmd.command = 0;
// 			ping_cmd.value1 = 0;
// 			ping_cmd.value2 = 0;
// 		}

// 		if (is_slave_device() == 0) {
// 			leds_frame.is_enabled = 1;

// 			if (systick_handleTimeout(&sec_time, 3000)) {
// 				blink(1);

// 				// remote_command_t remote_cmd1 = {
// 				// 	.command = 0xBB,
// 				// 	.value1 = command_step.values[command_step.index],
// 				// 	.value2 = 0xFFFFFFFF
// 				// };
// 				// // move to the next value
// 				// stepper32_tick(&command_step, 1);
// 				// printf("Sending value: %08X\n", remote_cmd1.value1);

// 				remote_command_t remote_cmd1 = {
// 					.command = 0xF1,
// 					.value1 = counter++,
// 					.value2 = sec_time
// 				};
// 				// printf("[Master] Sending value1: %u, value2: %u\n", 
// 				// 	remote_cmd1.value1, remote_cmd1.value2);

// 				modiSLER_loadCommand(&dataFrame, &remote_cmd1, sizeof(remote_cmd1));
// 				modiSLER_adv_data(&dataFrame);
// 			}
// 		}
		
// 		handle_receiving_frame(millis());
// 		button_task(&button, button_onChanged);
// 		// Neo_task();
// 	}
// }


#define REPORT_ALL 1 // if 0 only report received Find My advertisements

const uint8_t adv_data[] = {
		0x02, 0x0d, // header for LL: PDU + frame length
		0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // MAC (reversed)
		0x06, 0x09, 'R', 'X', ':', '?', '?'}; // 0x09: "Complete Local Name"

// On some chips, the iSLER buffer needs to be in a special section of memory.
// This section may be uninitialized, so we manually copy our initial data into it at runtime.
ISLER_BUF_ATTR uint8_t adv[256];

// BLE advertisements are sent on channels 37, 38 and 39
uint8_t adv_channels[] = {37,38,39};

uint8_t hex_lut[] = "0123456789ABCDEF";

void incoming_frame_handler() {
	// The chip stores the incoming frame in LLE_BUF, defined in extralibs/iSLER.h
	uint8_t *frame = (uint8_t*)LLE_BUF;
	int rssi = iSLERRSSI();

	// The first two bytes of the frame are metadata with PDU and length
	printf("RSSI:%d PDU:%d len:%d MAC:", rssi, frame[0], frame[1]);
	
	for(int i = 7; i > 2; i--) {
		printf("%02x:", frame[i]);
	}
	printf("%02x data:", frame[2]);
	for(int i = 8; i < frame[1] +2; i++) {
		printf("%02x ", frame[i]);
	}
	printf("\n");

	// advertise reception of a FindMy frame
	if(REPORT_ALL || (frame[8] == 0x1e && frame[10] == 0x4c)) {
		adv[sizeof(adv) -2] = hex_lut[(frame[7] >> 4)];
		adv[sizeof(adv) -1] = hex_lut[(frame[7] & 0xf)];
		for(int c = 0; c < sizeof(adv_channels); c++) {
			Frame_TX(ACCESS_ADDRESS, adv, sizeof(adv), adv_channels[c], PHY_MODE);
		}
	}
}

#define ROM_CFG_MAC_ADDR2		((const u32*)0x0007F018)

int main()
{
	SystemInit();

	funGpioInitAll();
	funPinMode( LED, GPIO_CFGLR_OUT_2Mhz_PP );

	iSLERInit(LL_TX_POWER_0_DBM);
	memcpy(adv, adv_data, sizeof(adv_data));

	blink(5);
	printf(".~ ch32fun iSLER ~.\n");

	printf("\n");
	printf("\n");
	uint8_t mac_addr[6];
	for (int i = 0; i < 6; i++) {
		mac_addr[i] = ((uint8_t *)ROM_CFG_MAC_ADDR2)[i];
	}

	printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", 
       mac_addr[0], mac_addr[1], mac_addr[2], 
       mac_addr[3], mac_addr[4], mac_addr[5]);

	// send out a first RX:?? advertisement to show we are alive
	for(int c = 0; c < sizeof(adv_channels); c++) {
		iSLERTX(ACCESS_ADDRESS, adv, sizeof(adv), adv_channels[c], PHY_MODE);
	}

	while(1) {
		// now listen for frames on channel 37 on bed6. When the RF subsystem
		// detects and finalizes one, "rx_ready" in iSLER.h is set true
		iSLERRX(ACCESS_ADDRESS, 37, PHY_MODE);
		while(!rx_ready);

		// we stepped over !rx_ready so we got a frame
		blink(1);
		incoming_frame_handler();
	}
}