#include "ch32fun.h"
#include "iSLER.h"
#include <stdio.h>

#ifdef CH570_CH572
#define LED PA9
#else
#define LED PA8
#endif

#define PHY_MODE                PHY_1M
#define MAX_PACKET_LEN          255
#define REPORT_ALL              1 // if 0 only report received Find My advertisements

typedef struct {
	uint16_t preamble;          // Sync pattern (0xAABB)
	uint16_t control_bits;      // control bits
	uint8_t group_id;
	uint8_t dest[6];            // destination

	uint16_t msgCode;           // message integrity check
	uint8_t data_len;           // length
	uint8_t payload[64];        // variable length
} MESS_txFrame_t;


// BLE advertisements are sent on channels 37, 38 and 39
uint8_t adv_channels[] = {37,38,39};
uint8_t frame_info[] = {0xff, 0x10}; // PDU, len, (maybe not?) needed in RX mode

uint8_t hex_lut[] = "0123456789ABCDEF";
uint8_t dev_name[] = "ch32fun888";


void blink(int n) {
	for(int i = n-1; i >= 0; i--) {
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms(33);
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if(i) Delay_Ms(33);
	}
}

void send_adv_data(uint8_t *data, size_t data_len) {
	if (!REPORT_ALL) return;
	blink(1);

	uint8_t adv_data[MAX_PACKET_LEN];

	uint8_t dev_header[] = {
		0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // MAC (reversed)
		// ---- Advertising Flags Field ----
		0x02, 						// Length
		0x01, 						// AD Type: Flags
		0x06, 						// LE General Discoverable Mode, BR/EDR Not Supported
		
		// ---- Complete Local Name Field  ----
		sizeof(dev_name) + 1, 		// Length: including the AD Type byte
		0x09, 						// AD Type: Complete Local Name
	};

	size_t adv_len = 0;

	//! copy from dev_header
	memcpy(&adv_data[adv_len], dev_header, sizeof(dev_header));
	adv_len += sizeof(dev_header);

	//! copy from dev_name
	memcpy(&adv_data[adv_len], dev_name, sizeof(dev_name));
	adv_len += sizeof(dev_name);

	//! copy from data_header
	uint8_t data_header[] = {
		// ---- Service Data Field ----
		data_len + 3, 			// Length: including the AD Type and Company ID bytes
		0xFF, 						// AD Type: Manufacturer Data
		0xD7, 0x07, 				// Company ID (WCH)
	};

	memcpy(&adv_data[adv_len], data_header, sizeof(data_header));
	adv_len += sizeof(data_header);

	//! copy from data
	memcpy(&adv_data[adv_len], data, data_len);
	adv_len += data_len;

	printf("adv len: %d\n", adv_len);

	for(int c = 0; c < sizeof(adv_channels); c++) {
		Frame_TX(adv_data, adv_len, adv_channels[c], PHY_MODE);
	}
}


int main()
{
	SystemInit();

	funGpioInitAll();
	funPinMode( LED, GPIO_CFGLR_OUT_2Mhz_PP );

	RFCoreInit(LL_TX_POWER_0_DBM);
	blink(5);
	printf(".~ ch32fun iSLER ~.\n");

	// send out a first RX:?? advertisement to show we are alive
	// for(int c = 0; c < sizeof(adv_channels); c++) {
	// 	Frame_TX(adv, sizeof(adv), adv_channels[c], PHY_MODE);
	// }

	while(1) {
		uint8_t data[] = "I like ble 777777";
		send_adv_data(data, sizeof(data));
        Delay_Ms(1000);

		// handle_frame_data();
		// send_frame_data();
		
		// // now listen for frames on channel 37. When the RF subsystem
		// // detects and finalizes one, "rx_ready" in iSLER.h is set true
		// Frame_RX(frame_info, 37, PHY_MODE);
		// while(!rx_ready);

		// // we stepped over !rx_ready so we got a frame
		// blink(1);
		// incoming_frame_handler();
	}
}
