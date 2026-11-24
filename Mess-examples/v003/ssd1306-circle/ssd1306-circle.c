#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 500u
#define BENCH_ROUNDS 5u
#define CIRCLE_RADIUS 8u
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

static M_Point circle_center(uint32_t i) {
	M_Point center = {
		(uint8_t)(12u + ((i * 17u) % 96u)),
		(uint8_t)(12u + ((i * 7u) % 40u)),
	};
	return center;
}

static uint32_t time_render_circle(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		render_circle(circle_center(i), CIRCLE_RADIUS, 0);

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_ssd1306_drawCircle(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		M_Point center = circle_center(i);
		ssd1306_drawCircle(center.x, center.y, CIRCLE_RADIUS, 1);
	}

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_render_filled_circle(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		render_circle(circle_center(i), CIRCLE_RADIUS, 1);

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_ssd1306_fillCircle(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		M_Point center = circle_center(i);
		ssd1306_fillCircle(center.x, center.y, CIRCLE_RADIUS, 1);
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

	uint32_t render_outline_ticks = 0;
	uint32_t draw_outline_ticks = 0;
	uint32_t render_filled_ticks = 0;
	uint32_t fill_circle_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_outline_ticks += time_render_circle();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		draw_outline_ticks += time_ssd1306_drawCircle();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_filled_ticks += time_render_filled_circle();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		fill_circle_ticks += time_ssd1306_fillCircle();
	}

	printf("SSD1306 circle benchmark\n");
	printf("%u calls/round, %u rounds, radius %u\n",
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS,
		(unsigned)CIRCLE_RADIUS);
	printf("outline render_circle:      %lu SysTick ticks/circle\n",
		(unsigned long)(render_outline_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("outline ssd1306_drawCircle: %lu SysTick ticks/circle\n",
		(unsigned long)(draw_outline_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("filled render_circle:       %lu SysTick ticks/circle\n",
		(unsigned long)(render_filled_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("filled ssd1306_fillCircle:  %lu SysTick ticks/circle\n",
		(unsigned long)(fill_circle_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	// Display one circle centered on the screen.
	memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
	render_circle((M_Point){ SSD1306_W / 2, SSD1306_H / 2 }, CIRCLE_RADIUS, 0);
	ssd1306_displayFrame();

	while (1) { }
}
