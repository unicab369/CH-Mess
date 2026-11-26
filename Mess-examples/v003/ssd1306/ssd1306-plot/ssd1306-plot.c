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
	return i2c_write_raw(&display, packet, (size_t)length + 1);
}

static const T_Point samples[] = {
	{ 11, 12 }, { 25, 21 }, { 39, 17 }, { 53, 34 },
	{ 67, 28 }, { 81, 42 }, { 95, 38 }, { 109, 50 }
};
static const T_Point swapped_samples[] = {
	{ 10, 8 }, { 15, 22 }, { 20, 35 }, { 25, 48 },
	{ 30, 60 }, { 35, 76 }, { 40, 92 }, { 55, 90 }
};

static T_SSD1306_Plot plot = {
	.x_label = "Sample X",
	.y_label = "title",
	.origin = { 5, 5 },
	.x_max = 120,
	.y_max = 60,
	.x_tick_interval = 14,
	.y_tick_interval = 8,
};

int main(void) {
	SystemInit();
	while (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not responding; retrying\n");
	}

	while (ssd1306_setup() != 0) {
		printf("SSD1306 setup failed; retrying\n");
		while (i2c_ping(display.addr) != I2C_OK) { }
	}
	memset(frame_buffer, 0, sizeof(frame_buffer));
	uint32_t layout_start = funSysTick32();
	if (!ssd1306_render_plot_layout(&plot)) while (1) { }

	uint32_t layout_ticks = (uint32_t)(funSysTick32() - layout_start);
	printf("ssd1306_render_plot_layout: %lu SysTick ticks\n", (unsigned long)layout_ticks);
	const uint8_t sample_count = sizeof(samples) / sizeof(samples[0]);

	ssd1306_render_graph_bars(&plot, samples, sample_count, 2);
	ssd1306_render_graph_line(&plot, samples, sample_count, 1);
	ssd1306_render_graph_markers(&plot, samples, sample_count, 2);
	ssd1306_displayPages(0, SSD1306_MAX_PAGE_IDX);
	Delay_Ms(1000);

	plot.swap_axes = 1;
	memset(frame_buffer, 0, sizeof(frame_buffer));
	ssd1306_render_plot_layout(&plot);
	const uint8_t swapped_sample_count = sizeof(swapped_samples) / sizeof(swapped_samples[0]);
	ssd1306_render_graph_bars(&plot, swapped_samples, swapped_sample_count, 2);
	ssd1306_render_graph_line(&plot, swapped_samples, swapped_sample_count, 1);
	ssd1306_render_graph_markers(&plot, swapped_samples, swapped_sample_count, 2);
	ssd1306_displayPages(0, SSD1306_MAX_PAGE_IDX);
	while (1) { }
}
