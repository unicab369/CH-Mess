#include "ch32fun.h"
#include <stdio.h>

#include "iSLER.h"

#define PHY_MODE       PHY_1M
#define ACCESS_ADDRESS 0x8E89BED6 // the "BED6" address for BLE advertisements

#include "modules/util_stepper.h"
#include "modules/util_print.h"
#include "modules/systick_irq.h"
#include "modules/modWS2812.h"
#include "modules/fun_button.h"
#include "ble_mesh.h"
#include "ble_mesh_crypto_test.h"


#ifdef CH570_CH572
#define LED PA9
#else
#define LED PA8
#define BUTTON_PIN 	PA4
#define INPUT1_PIN 	PA10
#define INPUT2_PIN 	PA5
#endif


void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms(33);
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if(i) Delay_Ms(33);
	}
}

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

// #define ROM_CFG_MAC_ADDR		((const u32*)0x0007F018)

int main()
{
	SystemInit();	//! required for millis()

	funGpioInitAll();
	funPinMode( LED, GPIO_CFGLR_OUT_2Mhz_PP );

	iSLERInit(LL_TX_POWER_0_DBM);
	memcpy(adv, adv_data, sizeof(adv_data));

	blink(5);
	printf(".~ ch32fun iSLER ~.\n");
	printf("micro-ecc self-test: %s\n", ble_mesh_test_ecc() == 0 ? "PASS" : "FAIL");
	printf("AES-CMAC self-test: %s\n", aes_cmac_test() == 0 ? "PASS" : "FAIL");


	// printf("\n");
	// printf("\n");
	// uint8_t mac_addr[6];
	// for (int i = 0; i < 6; i++) {
	// 	mac_addr[i] = ((uint8_t *)ROM_CFG_MAC_ADDR)[i];
	// }

	// printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
    //    mac_addr[0], mac_addr[1], mac_addr[2],
    //    mac_addr[3], mac_addr[4], mac_addr[5]);


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
