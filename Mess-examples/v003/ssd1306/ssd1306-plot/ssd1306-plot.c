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

#include "../../../../Mess-libs/i2c/fun_ssd1306_plot.h"

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

static const T_Point samples[] = {
	{ 16, 48 }, { 30, 39 }, { 44, 43 }, { 58, 26 },
	{ 72, 32 }, { 86, 18 }, { 100, 22 }, { 114, 10 }
};
static T_SSD1306_Plot plot = {
	.x_label = "Sample X",
	.y_label = "title",
	.origin = { 5, 5 },
	.x_max = 105,
	.y_max = 60,
	.x_tick = 14,
	.y_tick = 8,
};
static void draw_marker(T_Point point) {
	for (int8_t dy = -1; dy <= 1; ++dy) {
		for (int8_t dx = -1; dx <= 1; ++dx) {
			ssd1306_render_point((T_Point) {
				(uint8_t)((int16_t)point.x + dx),
				(uint8_t)((int16_t)point.y + dy)
			});
		}
	}
}

static void draw_plot(void) {
	for (uint8_t i = 1; i < sizeof(samples) / sizeof(samples[0]); ++i)
		ssd1306_render_line(samples[i - 1], samples[i], 1);
	for (uint8_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
		draw_marker(samples[i]);
}

int main(void) {
	SystemInit();
	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();
	memset(frame_buffer, 0, sizeof(frame_buffer));
	uint32_t layout_start = funSysTick32();
	if (!ssd1306_plot_render_layout(&plot))
		while (1) { }
	uint32_t layout_ticks = (uint32_t)(funSysTick32() - layout_start);
	printf("ssd1306_plot_render_layout: %lu SysTick ticks\n",
		(unsigned long)layout_ticks);
	draw_plot();
	ssd1306_displayPages(0, SSD1306_PAGES - 1);
	while (1) { }
}
