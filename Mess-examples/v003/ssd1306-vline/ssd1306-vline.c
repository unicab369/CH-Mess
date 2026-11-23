#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 1000u
#define BENCH_ROUNDS 5u
#define LINE_Y 4u
#define LINE_HEIGHT 56u
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

static uint32_t time_prefill_verLine(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		uint8_t x = (uint8_t)((i * 37u) % SSD1306_W);
		prefill_verLine(x, LINE_Y, LINE_Y + LINE_HEIGHT - 1u, 1);
	}

	return (uint32_t)(funSysTick32() - start);
}

static uint32_t time_drawFastVLine(void) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		uint32_t x = (i * 37u) % SSD1306_W;
		ssd1306_drawFastVLine(x, LINE_Y, LINE_HEIGHT, 1);
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

	uint32_t prefill_ticks = 0;
	uint32_t draw_ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		prefill_ticks += time_prefill_verLine();
	}

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(ssd1306_buffer, 0, sizeof(ssd1306_buffer));
		draw_ticks += time_drawFastVLine();
	}

	printf("SSD1306 vertical-line benchmark\n");
	printf("%u runs/round, %u rounds, %u pixels/line\n",
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS, (unsigned)LINE_HEIGHT);
	printf("prefill_verLine:       %lu SysTick ticks/call\n",
		(unsigned long)(prefill_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
	printf("ssd1306_drawFastVLine: %lu SysTick ticks/call\n",
		(unsigned long)(draw_ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	// The benchmark draws into RAM; send the finished frame to the OLED.
	ssd1306_renderFrame();

	while (1) { }
}
