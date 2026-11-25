#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 500u
#define BENCH_ROUNDS 5u
#define RECT_WIDTH 32u
#define RECT_HEIGHT 24u
#define RECT_AREA_WIDTH (RECT_WIDTH - 1u)
#define RECT_AREA_HEIGHT (RECT_HEIGHT - 1u)
#define SSD1306_SHARED_FRAME_BUFFER

i2c_device_t display = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,
	.regb = 1,
};

#include "../../../Mess-libs/i2c/ssd1306/fun_ssd1306.h"

uint8_t ssd1306_cmd(uint8_t command) {
	uint8_t packet[] = { 0x00, command };
	return i2c_write_raw(&display, packet, sizeof(packet));
}

uint8_t ssd1306_data(uint8_t *data, int length) {
	uint8_t packet[33];
	if (length < 0 || length > 32) return I2C_ERR_OVR;
	packet[0] = 0x40;
	memcpy(packet + 1, data, (size_t)length);
	return i2c_write_raw(&display, packet, (size_t)length + 1);
}

static uint8_t rect_x(uint32_t i) {
	return (uint8_t)(2u + ((i * 17u) % (SSD1306_W - RECT_WIDTH - 4u)));
}

static uint8_t rect_y(uint32_t i) {
	return (uint8_t)(2u + ((i * 7u) % (SSD1306_H - RECT_HEIGHT - 4u)));
}

static uint32_t time_render_rect(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		M_Point p0 = { rect_x(i), rect_y(i) };
		render_rect(p0, RECT_AREA_WIDTH, RECT_AREA_HEIGHT, 0);
	}

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_render_filled_rect(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		M_Point p0 = { rect_x(i), rect_y(i) };
		render_rect(p0, RECT_AREA_WIDTH, RECT_AREA_HEIGHT, 1);
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
	uint32_t render_filled_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_ticks += time_render_rect();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_filled_ticks += time_render_filled_rect();
	}

	printf("SSD1306 rectangle benchmark\n");
	printf("%u rectangles/round, %u rounds, %ux%u\n",
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS,
		(unsigned)RECT_WIDTH, (unsigned)RECT_HEIGHT);
	printf("outline render_rect:      %lu SysTick ticks/rectangle\n",
		(unsigned long)(render_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("filled render_rect:       %lu SysTick ticks/rectangle\n",
		(unsigned long)(render_filled_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	// Show an XOR rectangle cutting across the filled rectangle's lower-right corner.
	memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
	render_rect((M_Point){ 38, 14 }, 48, 34, 1);
	render_toggle_invert();
	render_rect((M_Point){ 70, 30 }, 24, 20, 1);
	render_toggle_invert();
	ssd1306_displayFrame();

	while (1) { }
}
