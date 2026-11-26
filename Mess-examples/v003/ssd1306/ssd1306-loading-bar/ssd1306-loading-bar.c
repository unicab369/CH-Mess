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

#include "../../../../Mess-libs/i2c/fun_ssd1306_graphic.h"

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
	SystemInit();
	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup(0x8F);
	const uint8_t x = 12, y = 8, height = 8, width = 104;
	const uint8_t y_bar_x = 60, y_bar_y = 24, y_bar_height = 32, y_bar_width = 8;
	uint8_t x_progress = 0, y_progress = 0;

	for (;;) {
		ssd1306_reset_progress(x, y, width, height, 1, &x_progress);
		ssd1306_reset_progress(y_bar_x, y_bar_y, y_bar_width, y_bar_height, 0, &y_progress);
		uint32_t start_ticks = funSysTick32();

		for (uint8_t percent = 0; percent <= 100u; ++percent) {
			ssd1306_show_percent(x, y, height, width, percent, 1, &x_progress);
			ssd1306_show_percent(y_bar_x, y_bar_y, y_bar_height, y_bar_width, percent, 0, &y_progress);
			// Delay_Ms(10);
		}
		uint32_t elapsed_ticks = (uint32_t)(funSysTick32() - start_ticks);
		printf("Loading bar 0-100%%: %lu SysTick ticks\n",
			(unsigned long)elapsed_ticks);
	}
}
