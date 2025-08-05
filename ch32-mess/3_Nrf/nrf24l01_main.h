#include "ch32fun.h"
#include "nrf24l01.h"

#define TIME_GAP 300
uint8_t ascending_number = 0;
char txt[16];

//######### debug fn
void uint8_to_binary_string(uint8_t value, char* output, int len) {
		for (int i = 0; i < len; i++) {
				output[len - i - 1] = (value & 1) ? '1' : '0';
				value >>= 1;
		}
		output[len] = '\0';
}


void print_reg(char* name, uint8_t addr) {
	char str[9];
	uint8_t REG;
	nrf24_read(addr, &REG, 1, CLOSE);
	uint8_to_binary_string(REG, str, 8);
	// printf("				 %s register: %s\n\r", name, str);
}


void print_debug() {
	print_reg("FEATURE      ", FEATURE_ADDRESS);
	print_reg("TX OBSERVE   ", OBSERVE_TX_ADDRESS);
	print_reg("STATUS       ", STATUS_ADDRESS);
	print_reg("RX_PW_P0 ADDR", RX_ADDR_P0_ADDRESS);
	print_reg("TX ADDR      ", TX_ADDR_ADDRESS);
	print_reg("EN_AA        ", EN_AA_ADDRESS);
	print_reg("EN_RXADDR    ", EN_RXADDR_ADDRESS);
}

//######### RX fn
void nrf_onReceive(uint8_t* data);

uint8_t recvnumber() {
	return nrf24_receive(&ascending_number, 1);
}

uint8_t recvstr() {
	return nrf24_receive((uint8_t*)&txt, 16);
}

void receive() {
	// to switch between sending an uint8_t and a 16-byte-char-array, just uncomment one of these two:
	//uint8_t result = recvnumber();
	// uint8_t result = recvstr();
	struct SensorData readings;
	uint8_t result = nrf24_receive((uint8_t*)&readings, sizeof(readings));

	// also uncomment the corresponding one for case OPERATION_DONE

	//print_debug();
	switch(result) {
		case OPERATION_ERROR:
			printf("EEE   RX operation error\n\r");
			break;
		case RECEIVE_FIFO_EMPTY:
			printf("      RX empty\n\r");
			//printf("      RX empty, last received: %u", ascending_number);
			break;
		case OPERATION_DONE:
         nrf_onReceive(&readings);
			// pick one of these two:
			printf("***   RX success, received: %u\n\r", ascending_number);
			// printf("***   RX success, received: %s\n\r", txt);
			break;
	}
	Delay_Ms(TIME_GAP);
}

//######### TX fn

uint8_t sendnumber() {
	return nrf24_transmit(&ascending_number, 1, ACK_MODE);
}

// function prototype (declaration), definition in "ch32v003fun.c"
int mini_snprintf(char* buffer, unsigned int buffer_len, const char *fmt, ...);

uint8_t sendstr() {
	mini_snprintf(txt, sizeof(txt), "Hello, %u", ascending_number);
	// printf("\n\rsending %s\n\r", txt);
	return nrf24_transmit((uint8_t*)txt, 16, ACK_MODE);
}

void send() {
	// to switch between sending an uint8_t and a 16-byte-char-array, just uncomment one of these two:
	//uint8_t tx_cmd_status = sendnumber();
	uint8_t tx_cmd_status = sendstr();
	switch (tx_cmd_status) {
		case TRANSMIT_BEGIN:
			// printf("***		sending package\n\r");
			break;
		case TRANSMIT_FAIL:
			// printf("EEE		unable to send package\n\r");
			break;
	}

	Delay_Ms(50);					// give the nRF some time to send
	// print_debug();

	switch (nrf24_transmit_status()) {
		case TRANSMIT_DONE:
			// printf("*OK		sent: %u\n\r", ascending_number);
			break;
		case TRANSMIT_FAILED:
			// printf("EEE		no ACK received!!\n\r");
			break;
		case TRANSMIT_IN_PROGRESS:
			// printf("EEE		still transmitting???\n\r");
			break;
	}
}

void sendData(void* data, size_t size) {
	uint8_t tx_cmd_status = nrf24_transmit((uint8_t*)data, size, ACK_MODE);
	
	switch (tx_cmd_status) {
		case TRANSMIT_BEGIN:
			// printf("***		sending package\n\r");
			break;
		case TRANSMIT_FAIL:
			// printf("EEE		unable to send package\n\r");
			break;
	}
	
	Delay_Ms(50);					// give the nRF some time to send
	// print_debug();

	switch (nrf24_transmit_status()) {
		case TRANSMIT_DONE:
			// printf("*OK		sent: %u\n\r", ascending_number);
			break;
		case TRANSMIT_FAILED:
			// printf("EEE		no ACK received!!\n\r");
			break;
		case TRANSMIT_IN_PROGRESS:
			// printf("EEE		still transmitting???\n\r");
			break;
	}
}

void nrf_setup(uint8_t isReceive) {
	if (isReceive == 1) {
		// printf("initializing radio as RX...");
		nrf24_device(RECEIVER, RESET);
	} else {
		// printf("initializing radio as TX...");
		nrf24_device(TRANSMITTER, RESET);
	}

	nrf24_rf_power(0);						//default TX power is -6dB, pretty strong, reduce to -18dBm for one room (ACK = TX)
	//nrf24_automatic_retransmit_setup(RETRANSMIT_DELAY_DEFAULT, 0);
	// printf("done.\n\r");
	print_debug();
}

void nrf_run(uint8_t isReceiver) {
   if (isReceiver == 1) {
      receive();
   } else {
      Delay_Ms(1000);
      send();
      ascending_number++;
   }

}