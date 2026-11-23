#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 1000u
#define BENCH_ROUNDS 5u
#define LINE_X 4u
#define LINE_WIDTH 120u
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

static uint8_t line_y(uint32_t i) {
	return (uint8_t)(1u + (i % (SSD1306_H - 2u)));
}

static uint32_t time_prefill_horLine(void) {
	Limit x_limit = { LINE_X, LINE_X + LINE_WIDTH - 1u };
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		prefill_horLine(line_y(i), x_limit, 1, 0);

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_drawFastHLine(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		ssd1306_drawFastHLine(LINE_X, line_y(i), LINE_WIDTH, 1);

	return (uint32_t)(funSysTick32() - start);
}

int main(void) {
	SystemInit();

	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();

	uint32_t prefill_ticks = 0;
	uint32_t draw_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));

		// Alternate order to reduce bias from always timing the same function first.
		if (round & 1u) {
			draw_ticks += time_drawFastHLine();
			prefill_ticks += time_prefill_horLine();
		} else {
			prefill_ticks += time_prefill_horLine();
			draw_ticks += time_drawFastHLine();
		}
	}

	printf("SSD1306 horizontal-line benchmark\n");
	printf("%u runs/round, %u rounds, %u pixels/line\n",
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS, (unsigned)LINE_WIDTH);
	printf("prefill_horLine:       %lu SysTick ticks/call\n",
		(unsigned long)(prefill_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("ssd1306_drawFastHLine: %lu SysTick ticks/call\n",
		(unsigned long)(draw_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	// Send the benchmark's final buffer contents to the OLED.
	ssd1306_renderFrame();

	while (1) { }
}
