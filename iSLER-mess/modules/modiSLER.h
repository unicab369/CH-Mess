#include "ch32fun.h"
#include "iSLER.h"
#include <stdio.h>

#define BLE_AD_MAC(mac) \
    (mac & 0xFF), (mac>>8) & 0xFF, \
    (mac>>16) & 0xFF, (mac>>24) & 0xFF, \
    (mac>>32) & 0xFF, (mac>>40) & 0xFF

#define BLE_AD_FLAGS(flags) 0x02, 0x01, flags

#define PRINT_HEX_ARRAY(arr) \
    do { \
        for(size_t i = 0; i < sizeof(arr); i++) { \
            printf("%02X ", arr[i]); \
        } \
        printf("\n"); \
    } while(0)


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
uint8_t dev_name[] = "ch32fun999";


void modiSLER_adv_data(uint8_t *data, size_t data_len) {
	if (!REPORT_ALL) return;
	uint8_t adv_data[MAX_PACKET_LEN];

	uint8_t dev_header[] = {
        // MAC address
        BLE_AD_MAC(0x112233445566),

		// ---- Advertising Flags Field ----
        BLE_AD_FLAGS(0x06), // LE General Discoverable Mode, BR/EDR Not Supported
		
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

	// printf("adv len: %d\n", adv_len);

	for(int c = 0; c < sizeof(adv_channels); c++) {
		Frame_TX(adv_data, adv_len, adv_channels[c], PHY_MODE);
	}
}