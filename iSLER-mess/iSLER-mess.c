#include "ch32fun.h"
#include <stdio.h>

#include "modules/util_print.h"
#include "modules/systick_irq.h"
#include "modules/modWS2812.h"
#include "modules/modiSLER.h"

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

remote_command_t remote_cmd1 = {
	.command = 0xBB,
	.value = 0x1B2B3B4B,
};

void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms(33);
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if(i) Delay_Ms(33);
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
	modiSLER_loadData(&dataFrame, &remote_cmd1, sizeof(remote_cmd1));

	uint32_t sec_time = 0;
	uint8_t frame_info[] = {0xff, 0x10}; // PDU, len, (maybe not?) needed in RX mode

	while(1) {
		if (funDigitalRead(PA10) == FUN_LOW) {
			move_leds.is_enabled = 1;

			if (systick_handleTimeout(&sec_time, 5000)) {
				// uint8_t data[] = "I like ble 777777";
				modiSLER_adv_data(&dataFrame);
				blink(1);
			}
		} else {
			// now listen for frames on channel 37. When the RF subsystem
			// detects and finalizes one, "rx_ready" in iSLER.h is set true
			Frame_RX(frame_info, 37, PHY_MODE);
			while(!rx_ready);

			// we stepped over !rx_ready so we got a frame
			if (modiSLER_rx_handler()) {
				move_leds.is_enabled = 1;
				move_leds.ref_index = 0;
				move_leds.ref_time = millis();
				blink(1);
			}
			
			// if (rx_ready) {
			// 	if (modiSLER_rx_handler()) {
			// 		blink(1);
			// 	}
			// }
		}

        Neo_task();
	}
}
