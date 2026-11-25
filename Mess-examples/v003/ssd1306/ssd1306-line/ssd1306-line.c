#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib_i2c.h"

#define BENCH_RUNS 100
#define BENCH_ROUNDS 5u
#define THICK_LINE_WIDTH 3u
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
	return i2c_write_raw(&display, packet, (size_t)length + 1);
}

// A mix of horizontal, vertical, shallow, steep, and diagonal lines.
static const T_Point line_points[] = {
	{ 4,  4 }, { 123,  4 },
	{ 4, 10 }, {   4, 59 },
	{ 4, 16 }, { 123, 32 },
	{ 4, 59 }, {  45,  4 },
	{ 4, 59 }, { 123, 59 },
	{ 64,  4 }, {  91, 59 },
};

#define LINE_COUNT (sizeof(line_points) / (2u * sizeof(line_points[0])))

static uint32_t time_render_line(void) {
	uint32_t start = funSysTick32();

	for (uint32_t run = 0; run < BENCH_RUNS; ++run) {
		for (uint8_t line = 0; line < LINE_COUNT; ++line) {
			T_Point p0 = line_points[line * 2u];
			T_Point p1 = line_points[line * 2u + 1u];
			render_line(p0, p1, 1);
		}
	}

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_render_thick_line(void) {
	uint32_t start = funSysTick32();

	for (uint32_t run = 0; run < BENCH_RUNS; ++run) {
		for (uint8_t line = 0; line < LINE_COUNT; ++line) {
			T_Point p0 = line_points[line * 2u];
			T_Point p1 = line_points[line * 2u + 1u];
			render_line(p0, p1, THICK_LINE_WIDTH);
		}
	}

	return (uint32_t)(funSysTick32() - start);
}

int main(void) {
	SystemInit();

	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();

	uint32_t render_ticks = 0;
	uint32_t render_thick_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render_ticks += time_render_line();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render_thick_ticks += time_render_thick_line();
	}

	printf("SSD1306 line benchmark\n");
	printf("%u lines/set, %u sets/round, %u rounds\n",
		(unsigned)LINE_COUNT, (unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS);
	printf("render_line:       %lu SysTick ticks/line\n",
		(unsigned long)(render_ticks / (LINE_COUNT * BENCH_RUNS * BENCH_ROUNDS)));
	printf("render_line width %u: %lu SysTick ticks/line\n",
		(unsigned)THICK_LINE_WIDTH,
		(unsigned long)(render_thick_ticks / (LINE_COUNT * BENCH_RUNS * BENCH_ROUNDS)));

	// Show a thin line set and a separate thick render_line sample.
	memset(frame_buffer, 0, sizeof(frame_buffer));
	for (uint8_t line = 0; line < LINE_COUNT; ++line) {
		T_Point p0 = line_points[line * 2u];
		T_Point p1 = line_points[line * 2u + 1u];
		render_line(p0, p1, 1);
	}
	render_line((T_Point){ 8, 42 }, (T_Point){ 120, 58 }, THICK_LINE_WIDTH);
	ssd1306_displayPages(0, SSD1306_PAGES - 1);

	while (1) { }
}
