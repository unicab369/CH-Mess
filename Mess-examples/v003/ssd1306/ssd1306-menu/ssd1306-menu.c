#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib_i2c.h"

#define SSD1306_SHARED_FRAME_BUFFER

i2c_device_t display = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,
	.regb = 1,
};

#include "../../../../Mess-libs/i2c/fun_ssd1306.h"

uint8_t SSD1306_CMD(uint8_t command) {
	uint8_t packet[] = { 0x00, command };
	return i2c_write_raw(&display, packet, sizeof(packet));
}

uint8_t SSD1306_DATA(uint8_t *data, int length) {
	uint8_t packet[33];
	if (length < 0 || length > 32) return I2C_ERR_OVR;
	packet[0] = 0x40;
	memcpy(packet + 1, data, (size_t)length);
	return i2c_write_raw(&display, packet, (size_t)length + 1u);
}

int main(void) {
	static const char *const items[] = { "START", "SETTINGS", "ABOUT", "EXIT" };
	const uint8_t item_count = sizeof(items) / sizeof(items[0]);
	uint8_t selected = 0;

	SystemInit();
	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();
	for (;;) {
		ssd1306_render_str_line("MAIN MENU", 0, 0, 1);
		for (uint8_t i = 0; i < item_count; ++i) {
			char line[SSD1306_STR_SIZE + 1u];
			snprintf(line, sizeof(line), "%c %s", i == selected ? '>' : ' ', items[i]);
			ssd1306_render_str_line(line, i + 1u, 0, 1);
		}
		ssd1306_displayPages(0, SSD1306_MAX_PAGE_IDX);

		Delay_Ms(1000);
		selected = (selected + 1u) % item_count;
	}
}
