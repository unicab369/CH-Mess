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


#ifndef PACKED
#define PACKED __attribute__( ( packed ) )
#endif

typedef struct PACKED {
	uint16_t preamble;          // Sync pattern (0xAABB)
	uint16_t control_bits;      // control bits
	uint16_t msgCode;           // message integrity check
	uint8_t dest[6];            // destination

	uint8_t group_id;
	uint8_t data_len;           // length
	uint8_t payload[64];        // variable length
} MESS_DataFrame_t;

typedef struct PACKED {
	uint8_t command;
	uint32_t value;
} remote_command_t;

typedef struct PACKED {
	uint8_t mac[6];
	uint8_t field_adv_flags[3];
	uint8_t name_len;
	uint8_t ad_type_local_name;
	uint8_t name[20];
	uint8_t data_len;
	uint8_t field_sev_data[3];
	MESS_DataFrame_t dataFrame;
} iSLER_frame_t;

// BLE advertisements are sent on channels 37, 38 and 39
uint8_t adv_channels[] = {37, 38, 39};
uint8_t dev_name[] = "ch32fun999";

void modiSLER_loadData(MESS_DataFrame_t *dataFrame, uint8_t *data, size_t data_len) {
	dataFrame->data_len = data_len;
	memcpy(dataFrame->payload, data, data_len);
}

void modiSLER_adv_data(MESS_DataFrame_t *dataFrame) {
	if (!REPORT_ALL) return;
	uint8_t data_len = sizeof(MESS_DataFrame_t);

	iSLER_frame_t frame = {
		.mac = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
		.field_adv_flags = {0x02, 0x01, 0x06},
		.name_len = 21,
		.ad_type_local_name = 0x09,
		.name = { 'b','e', 'e', '-', '6', '6', '6' },
		.data_len = data_len+3,
		.field_sev_data = {0xFF, 0xD7, 0x07},
		.dataFrame = *dataFrame
	};
	

	// uint8_t adv_data[MAX_PACKET_LEN];

	// uint8_t dev_header[] = {
    //     // MAC address
    //     BLE_AD_MAC(0x112233445566),

	// 	// ---- Advertising Flags Field ----
    //     BLE_AD_FLAGS(0x06), // LE General Discoverable Mode, BR/EDR Not Supported
		
	// 	// ---- Complete Local Name Field  ----
	// 	sizeof(dev_name) + 1, 		// Length: including the AD Type byte
	// 	0x09, 						// AD Type: Complete Local Name
	// };

	// size_t adv_len = 0;

	// //! copy from dev_header
	// memcpy(&adv_data[adv_len], dev_header, sizeof(dev_header));
	// adv_len += sizeof(dev_header);

	// //! copy from dev_name
	// memcpy(&adv_data[adv_len], dev_name, sizeof(dev_name));
	// adv_len += sizeof(dev_name);

	// //! copy from data_header
	// uint8_t data_header[] = {
	// 	// ---- Service Data Field ----
	// 	data_len + 3, 				// Length: including the AD Type and Company ID bytes
	// 	0xFF, 						// AD Type: Manufacturer Data
	// 	0xD7, 0x07, 				// Company ID (WCH)
	// };

	// memcpy(&adv_data[adv_len], data_header, sizeof(data_header));
	// adv_len += sizeof(data_header);

	// //! copy from data
	// memcpy(&adv_data[adv_len], data, data_len);
	// adv_len += data_len;

	// printf("adv len: %d\n", adv_len);

	// for(int c = 0; c < sizeof(adv_channels); c++) {
	// 	Frame_TX(adv_data, adv_len, adv_channels[c], PHY_MODE);
	// }

	PRINT_STRUCT_BYTES(&frame, "%02X");
	printf("\n");

	for(int c = 0; c < sizeof(adv_channels); c++) {
		Frame_TX((uint8_t*)&frame, sizeof(frame), adv_channels[c], PHY_MODE);
	}
}

uint8_t modiSLER_print_rawData(uint8_t *data, size_t data_len) {
	
}

uint8_t modiSLER_rx_handler() {
	// The chip stores the incoming frame in LLE_BUF, defined in extralibs/iSLER.h
	uint8_t *frame = (uint8_t*)LLE_BUF;
	
	uint8_t sender_mac[6];
	for(int i= 0;i<6; i++) sender_mac[i] = frame[7-i];

	uint8_t target_mac[] = { BLE_AD_MAC(0x112233445566) };

	// printf("sender_mac\n");
	// PRINT_ARRAY(sender_mac, "%02x");

	if (memcmp(sender_mac, target_mac, 6) == 0) {
		// first 8 bytes contains: [RSSI x 1Byte] [len x 1Byte] [MAC x 6Bytes]
		// The first two bytes of the frame are metadata with RSSI and length
		PRINT_SEPARATOR();
		printf("RSSI:%d len:%d MAC:", frame[0], frame[1]);
		PRINT_ARRAY(sender_mac, "%02X");
		printf("Raw Data: ");
		PRINT_ARRAY_WITH_SIZE(frame, frame[1], "%02X");


		iSLER_frame_t* rx_frame = (iSLER_frame_t*)LLE_BUF;

		PRINT_STRUCT_BYTES(rx_frame, "%02X");

		// uint8_t name_len_idx = 8+3;	// first 8 bytes + len(Advertising Flags Field)
		// uint8_t name_len = frame[name_len_idx];
		// printf("name idx: %u len: %u\n", name_len_idx, name_len);
	
		// uint8_t name_str_idx = name_len_idx + 2;	// AD Type: Local name 0x09
		// printf("name str: ");
		// for(int i= name_str_idx; i < name_str_idx + name_len - 1; i++) {
		// 	printf("%c", frame[i]);
		// }
		// printf("\n");

		// uint8_t tx_len_idx = name_str_idx + name_len - 1;
		// uint8_t tx_len = frame[tx_len_idx];
		// printf("\ntx idx: %u, len: %02X\n", tx_len_idx, tx_len);

		// printf("tx data: ");
		// // follow by AD Type and Company ID (3 bytes)
		// for(int i= tx_len_idx + 4; i < tx_len_idx + 4 + tx_len; i++) {
		// 	printf("%02X ", frame[i]);
		// }
		// printf("\n");

		// printf("\nMESS_txFrame:\n");
		// uint8_t frame_data[sizeof(MESS_DataFrame_t)] = {0};
		// memcpy(frame_data, frame + tx_len_idx + 4, tx_len);

		// // MESS_DataFrame_t receiv_frame;
		// // memcpy(&receiv_frame, frame + tx_len_idx + 4, sizeof(MESS_DataFrame_t));
		// // PRINT_STRUCT_BYTES(&receiv_frame, "%02X");

		// MESS_DataFrame_t *rx = frame_data;
		// PRINT_STRUCT_BYTES(rx, "%02X");

		return 1;
	}

	return 0;
}