#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib_i2c.h"

#define BENCH_RUNS 10
#define BENCH_ROUNDS 5u
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

static T_Point star[] = {
	{ 64, 5 }, { 78, 25 }, { 102, 25 }, { 83, 40 }, { 90, 59 },
	{ 64, 47 }, { 38, 59 }, { 45, 40 }, { 26, 25 }, { 50, 25 }
};

static uint32_t time_render_star(void) {
	uint32_t start = funSysTick32();
	for (uint32_t run = 0; run < BENCH_RUNS; ++run)
		ssd1306_render_solid_poly(star, (uint8_t)(sizeof(star) / sizeof(star[0])));
	return (uint32_t)(funSysTick32() - start);
}

int main(void) {
	SystemInit();
	uint8_t i2c_status = i2c_init(&display);
	if (i2c_status != I2C_OK) {
		printf("I2C init failed\n");
		while (1) { }
	}
	i2c_status = i2c_ping(display.addr);
	if (i2c_status != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();
	memset(frame_buffer, 0, sizeof(frame_buffer));
	ssd1306_render_solid_poly(star, (uint8_t)(sizeof(star) / sizeof(star[0])));
	ssd1306_displayPages(0, SSD1306_MAX_PAGE_IDX);

	uint32_t ticks = 0;
	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		ticks += time_render_star();
	}
	printf("SSD1306 solid polygon benchmark\n");
	printf("star: %u vertices, %u calls/round, %u rounds\n",
		(unsigned)(sizeof(star) / sizeof(star[0])),
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS);
	printf("ssd1306_render_solid_poly: %lu SysTick ticks/star\n",
		(unsigned long)(ticks / (BENCH_RUNS * BENCH_ROUNDS)));

	while (1) { }
}
