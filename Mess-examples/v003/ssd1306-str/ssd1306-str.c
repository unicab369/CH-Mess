#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 25u
#define BENCH_ROUNDS 5u
#define SSD1306_SHARED_FRAME_BUFFER

static char text_line[] = "0123456789ABCDEF";

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

static uint32_t time_render_str(void) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		render_str(text_line, 0, 0, &font_7x5);
		__asm__ volatile("" ::: "memory");
	}
	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_render_str_8x8(void) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		render_str(text_line, 0, 0, &font_8x8);
		__asm__ volatile("" ::: "memory");
	}
	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_display_str(void) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		display_str(text_line, 0, 0);
		__asm__ volatile("" ::: "memory");
	}
	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_drawstr(void) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		ssd1306_drawstr(0, 0, text_line, 1);
		__asm__ volatile("" ::: "memory");
	}
	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_drawstr_font(const font_t *font) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		ssd1306_drawstr_font(0, 0, text_line, font);
		__asm__ volatile("" ::: "memory");
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
	uint32_t render_8x8_ticks = 0;
	uint32_t display_ticks = 0;
	uint32_t draw_ticks = 0;
	uint32_t draw_7x5_ticks = 0;
	uint32_t draw_8x8_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_ticks += time_render_str();
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_8x8_ticks += time_render_str_8x8();
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		display_ticks += time_display_str();
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		draw_ticks += time_drawstr();
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		draw_7x5_ticks += time_drawstr_font(&font_7x5);
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		draw_8x8_ticks += time_drawstr_font(&font_8x8);
	}

	printf("SSD1306 framebuffer string benchmark\n");
	printf("%u calls/round, %u rounds\n", (unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS);
	printf("render_str (FONT_7x5):   %lu ticks/string\n",
		(unsigned long)(render_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("render_str (FONT_8x8):   %lu ticks/string\n",
		(unsigned long)(render_8x8_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("display_str (I2C):       %lu ticks/string\n",
		(unsigned long)(display_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("ssd1306_drawstr:         %lu ticks/string\n",
		(unsigned long)(draw_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("drawstr_font (FONT_7x5): %lu ticks/string\n",
		(unsigned long)(draw_7x5_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("drawstr_font (FONT_8x8): %lu ticks/string\n",
		(unsigned long)(draw_8x8_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
	render_str(text_line, 0, 0, &font_7x5);
	render_str(text_line, 0, 16, &font_8x8);
	ssd1306_drawstr_font(0, 32, text_line, &font_7x5);
	ssd1306_drawstr_font(0, 48, text_line, &font_8x8);
	ssd1306_refresh();
	while (1) { }
}
