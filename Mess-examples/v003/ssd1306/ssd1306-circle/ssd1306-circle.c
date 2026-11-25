#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib_i2c.h"

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

static T_Point circle_center(uint32_t i) {
	T_Point center = {
		(uint8_t)(12u + ((i * 17u) % 96u)),
		(uint8_t)(12u + ((i * 7u) % 40u)),
	};
	return center;
}

static uint32_t time_render_circle(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		ssd1306_render_circle(circle_center(i), CIRCLE_RADIUS, 0);

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_render_filled_circle(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		ssd1306_render_circle(circle_center(i), CIRCLE_RADIUS, 1);

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
	uint32_t render_filled_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render_outline_ticks += time_render_circle();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render_filled_ticks += time_render_filled_circle();
	}

	printf("SSD1306 circle benchmark\n");
	printf("%u calls/round, %u rounds, radius %u\n",
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS,
		(unsigned)CIRCLE_RADIUS);
	printf("outline ssd1306_render_circle:      %lu SysTick ticks/circle\n",
		(unsigned long)(render_outline_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("filled ssd1306_render_circle:       %lu SysTick ticks/circle\n",
		(unsigned long)(render_filled_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	// Show an outline above a filled circle.
	memset(frame_buffer, 0, sizeof(frame_buffer));
	ssd1306_render_circle((T_Point){ 64, 16 }, CIRCLE_RADIUS, 0);
	ssd1306_render_circle((T_Point){ 64, 48 }, CIRCLE_RADIUS, 1);
	ssd1306_displayPages(0, SSD1306_PAGES - 1);

	while (1) { }
}
