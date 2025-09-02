#include "ch32fun.h"

#include "../Mess-libs/i2c/lib_i2c.h"
#include "../Mess-libs/i2c/ssd1306/fun_ssd1306.h"

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

#define PRINT_BUFF_SIZE 2

char str_output[SSD1306_STR_SIZE];

i2c_device_t dev_ssd1306 = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,				// Default address for SSD1306
	.regb = 1,
};

/* send OLED command byte */
uint8_t ssd1306_cmd(uint8_t cmd) {
	uint8_t pkt[2];
	pkt[0] = 0;
	pkt[1] = cmd;
	return i2c_write_raw(&dev_ssd1306, pkt, 2);
}

/* send OLED data packet (up to 32 bytes) */
uint8_t ssd1306_data(uint8_t *data, int sz) {
	uint8_t pkt[33];
	pkt[0] = 0x40;
	memcpy(&pkt[1], data, sz);
	return i2c_write_raw(&dev_ssd1306, pkt, sz+1);
}


void modI2C_display(const char *str, uint8_t line) {
	//! validate device before print
	if (i2c_ping(0x3C) != I2C_OK) return;
	ssd1306_print_str_at(str, line, 0);
}

void i2c_scan_callback(const uint8_t addr) {
	if (addr == 0x00 || addr == 0x7F) return; // Skip reserved addresses
	
	static int line = 1;
	sprintf(str_output, "I2C: 0x%02X", addr);
	printf("%s\n", str_output);
	modI2C_display(str_output, line++);
}

void modI2C_setup() {
	if(i2c_init(&dev_ssd1306) != I2C_OK) {
		printf("Failed to init I2C\n");
	} 
	else {
		if (i2c_ping(0x3C) == I2C_OK) {
			ssd1306_setup();

			sprintf(str_output, "Hello Bee!");
			ssd1306_print_str_at(str_output, 0, 0);
		}

		// Scan the I2C Bus, prints any devices that respond
		printf("----Scanning I2C Bus for Devices---\n");
		i2c_scan(i2c_scan_callback);
		printf("----Done Scanning----\n\n");
	}
}


typedef struct PACKED {
	char str[SSD1306_STR_SIZE];
	uint8_t line_num;		// line_num = 0 means empty
} PrintBuff_t;


uint32_t lock_printTime = 0;
uint8_t printBuff_idx = 0;
PrintBuff_t printBuff[PRINT_BUFF_SIZE] = { 0 };
uint8_t flag_printBuff_data = 1;					// set to 1 to allow initial print

void mngI2c_load_printBuff(const char *str, uint8_t line_idx) {
	flag_printBuff_data = 1;
	PrintBuff_t *buff = &printBuff[printBuff_idx];
	buff->line_num = line_idx + 1;
	strncpy(buff->str, str, SSD1306_STR_SIZE);
	// buff->str[24] = '\0';
	printBuff_idx = (printBuff_idx + 1) % PRINT_BUFF_SIZE;
}

uint32_t line6_preserve_time = 0;

void mngI2c_load_joystick(uint32_t time, uint16_t x, uint16_t y) {
	// keep line6_preserve_time shown for at least 1 second
	if (time - line6_preserve_time < 1000) return;

	sprintf(str_output, "jx: %d, jy: %d", x, y);
	mngI2c_load_printBuff(str_output, 6);
}

void mngI2c_load_buttonState(uint32_t time, uint8_t state) {
	line6_preserve_time = time;

	sprintf(str_output, "button: %d", state);
	mngI2c_load_printBuff(str_output, 6);
}

void mngI2c_load_encoder(uint32_t time, uint8_t pos, uint8_t dir) {
	line6_preserve_time = time;

	sprintf(str_output, "pos: %d, dir: %s", pos, dir ? "CW" : "CCW");
	mngI2c_load_printBuff(str_output, 6);
}

char loading_char[4] = "\\";

void cycle_loading_char() {
    // Since we're only dealing with single characters, we can just check the first byte.
    // This is faster and safer than strcmp/strcpy.
    switch(loading_char[0]) {
        case '\\':	strcpy(loading_char, "|"); break;
        case '|':	strcpy(loading_char, "/"); break;
        case '/':	strcpy(loading_char, "-"); break;
        case '-':	strcpy(loading_char, "\\"); break;
    }
}

void mngI2c_loadCounter(uint32_t counter, uint32_t runTime) {
	sprintf(str_output, "%s cyc/s %lu ~ %lums", loading_char, counter, runTime);
	mngI2c_load_printBuff(str_output, 7);
	cycle_loading_char();
}

void mngI2c_printBuff_task() {
	if (!flag_printBuff_data) return;

	for (int i = 0; i < PRINT_BUFF_SIZE; i++) {
		if (printBuff[i].line_num == 0) continue;
		modI2C_display(printBuff[i].str, printBuff[i].line_num-1);
	}

	//! Clear printBuff
	memset(printBuff, 0, sizeof(printBuff));
	flag_printBuff_data = 0;
}