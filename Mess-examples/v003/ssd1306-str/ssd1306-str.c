#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 6
#define BENCH_ROUNDS 5u
#define SSD1306_SHARED_FRAME_BUFFER

static char text_line[] = "0123456789ABCDEF";
static char scaled_text[] = "01234567";

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

static uint32_t time_render_str_scaled(const font_t *font) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		render_str_scaled(scaled_text, 0, 0, font, 2);
		__asm__ volatile("" ::: "memory");
	}
	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_render_str_line(void) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		render_str_line(text_line, 0, 0);
		ssd1306_displayArea(0, 0, 0, SSD1306_STR_SIZE * 5u);
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
	uint32_t scaled_7x5_ticks = 0;
	uint32_t scaled_8x8_ticks = 0;
	uint32_t line_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_ticks += time_render_str();
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		render_8x8_ticks += time_render_str_8x8();
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		scaled_7x5_ticks += time_render_str_scaled(&font_7x5);
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		scaled_8x8_ticks += time_render_str_scaled(&font_8x8);
	}
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		line_ticks += time_render_str_line();
	}

	printf("SSD1306 framebuffer string benchmark\n");
	printf("%u calls/round, %u rounds\n", (unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS);
	printf("render_str (FONT_7x5):   %lu ticks/string\n",
		(unsigned long)(render_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("render_str (FONT_8x8):   %lu ticks/string\n",
		(unsigned long)(render_8x8_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("scaled 2x (7x5, 8 chars): %lu ticks/string\n",
		(unsigned long)(scaled_7x5_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("scaled 2x (8x8, 8 chars): %lu ticks/string\n",
		(unsigned long)(scaled_8x8_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("render_str_line + area:  %lu ticks/string\n",
		(unsigned long)(line_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
	render_str(text_line, 0, 0, &font_7x5);
	render_str(text_line, 0, 16, &font_8x8);
	render_str_scaled(scaled_text, 0, 32, &font_7x5, 2);
	render_str_scaled(scaled_text, 0, 48, &font_8x8, 2);
	ssd1306_displayArea(0, SSD1306_PAGES - 1, 0, SSD1306_W);
	while (1) { }
}
