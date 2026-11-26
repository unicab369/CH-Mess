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
	static const char *const items[] = {
		"START", "SETTINGS", "ABOUT", "DISPLAY", "SOUND",
		"NETWORK", "TOOLS", "STATUS", "HELP", "EXIT",
	};
	T_Menu menu = {
		.items = items,
		.total_count = sizeof(items) / sizeof(items[0]),
		.start_page = 1,
		.position = 0,
		.visible_count = 6, // SSD1306_PAGES,
	};

	SystemInit();
	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup(0xFF);
	ssd1306_display_menu(&menu);
	for (;;) {
		Delay_Ms(1000);
		int8_t offset = menu.position == menu.total_count - 1u
			? -(int8_t)menu.position : 1;
		uint32_t start_ticks = funSysTick32();
		ssd1306_menu_move(&menu, offset);
		uint32_t elapsed_ticks = (uint32_t)(funSysTick32() - start_ticks);
		printf("Menu move: %lu SysTick ticks\n", (unsigned long)elapsed_ticks);
	}
}
