#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define SSD1306_SHARED_FRAME_BUFFER

i2c_device_t display = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,
	.regb = 1,
};

#include "../../../Mess-libs/i2c/ssd1306/fun_ssd1306.h"

uint8_t SSD1306_CMD(uint8_t command) {
	uint8_t packet[] = { 0x00, command };
	return i2c_write_raw(&display, packet, sizeof(packet));
}

uint8_t SSD1306_DATA(uint8_t *data, int length) {
	uint8_t packet[33];
	if (length < 0 || length > 32) return I2C_ERR_OVR;
	packet[0] = 0x40;
	memcpy(packet + 1, data, (size_t)length);
	return i2c_write_raw(&display, packet, (size_t)length + 1);
}

int main(void) {
	SystemInit();

	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();
	render_str_line("sz 8x8 testing", 0, 0);
	render_str_line("testing 22222234fdafadfafa", 1, 0);
	render_str_line("testing 33334fdafadfafa", 2, 0);
	render_str_line("testing 44444fdafadfafa", 3, 0);
	render_str_line("testing 55554fdafadfafa", 4, 0);
	render_str_line("testing 66664fdafadfafa", 5, 0);
	render_str_line("testing 77774fdafadfafa", 6, 0);
	render_str_line("testing 88884fdafadfafa", 7, 0);
	ssd1306_displayArea(0, 7, 0, SSD1306_STR_SIZE * 5u);
	while (1) { }
}
