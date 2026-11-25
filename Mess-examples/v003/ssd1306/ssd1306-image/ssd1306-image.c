#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib/lib_i2c.h"

#define BENCH_RUNS 6u
#define BENCH_ROUNDS 5u
#define SSD1306_SHARED_FRAME_BUFFER
#define SSD1306_FULLUSE

i2c_device_t display = {
	.clkr = I2C_CLK_100KHZ,
	.type = I2C_ADDR_7BIT,
	.addr = 0x3C,
	.regb = 1,
};

#include "../../../../Mess-libs/i2c/ssd1306/fun_ssd1306.h"
#include "../../../../Mess-libs/i2c/ssd1306/bomb.h"

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

static uint32_t time_render_image(void) {
	uint32_t start = funSysTick32();
	for (uint32_t i = 0; i < BENCH_RUNS; ++i) {
		render_image(bomb_i_stripped, 0, 0, 32, 32);
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
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render_ticks += time_render_image();
	}

	printf("SSD1306 image renderer benchmark\n");
	printf("%u calls/round, %u rounds, 32x32 bitmap\n", (unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS);
	printf("render_image:      %lu ticks/image\n", (unsigned long)(render_ticks / (BENCH_RUNS * BENCH_ROUNDS)));
		memset(frame_buffer, 0, sizeof(frame_buffer));
	render_image(bomb_i_stripped, 0, 16, 32, 32);
	ssd1306_displayArea(0, SSD1306_PAGES - 1, 0, SSD1306_W);
	while (1) { }
}
