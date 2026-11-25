#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib_i2c.h"

#define BENCH_RUNS 10
#define BENCH_ROUNDS 5u
#define PIE_RADIUS 28u
#define PIE_START_ANGLE 0
#define PIE_END_ANGLE 230
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

static T_Point pie_center(uint32_t i) {
	// Keep the entire radius inside the 128x64 framebuffer.
	T_Point center = {
		(uint8_t)(32u + ((i * 17u) % 64u)),
		(uint8_t)(28u + ((i * 7u) % 8u)),
	};
	return center;
}

static uint32_t time_render_pie(void (*render)(T_Point, uint8_t, int16_t, int16_t)) {
	uint32_t start = funSysTick32();

	for (uint32_t i = 0; i < BENCH_RUNS; ++i)
		render(pie_center(i), PIE_RADIUS, PIE_START_ANGLE, PIE_END_ANGLE);

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
	uint32_t render7_ticks = 0;

	for (uint32_t round = 0; round < BENCH_ROUNDS; ++round) {
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render_ticks += time_render_pie(ssd1306_render_pie);
		memset(frame_buffer, 0, sizeof(frame_buffer));
		render7_ticks += time_render_pie(ssd1306_render_pie_fast);
	}

	printf("SSD1306 pie renderer benchmark\n");
	printf("%u calls/round, %u rounds, radius %u, angles %u..%u\n",
		(unsigned)BENCH_RUNS, (unsigned)BENCH_ROUNDS,
		(unsigned)PIE_RADIUS,
		(unsigned)PIE_START_ANGLE, (unsigned)PIE_END_ANGLE);
	printf("ssd1306_render_pie:  %lu SysTick ticks/pie\n",
		(unsigned long)(render_ticks / (BENCH_RUNS * BENCH_ROUNDS)));




	printf("ssd1306_render_pie_fast: %lu SysTick ticks/pie\n",
		(unsigned long)(render7_ticks / (BENCH_RUNS * BENCH_ROUNDS)));






	memset(frame_buffer, 0, sizeof(frame_buffer));
	// Show two filled 230-degree pies side by side.
	ssd1306_render_pie_fast((T_Point){ 32, 32 }, PIE_RADIUS, PIE_START_ANGLE, PIE_END_ANGLE);
	ssd1306_render_pie_fast((T_Point){ 96, 32 }, PIE_RADIUS, PIE_START_ANGLE, PIE_END_ANGLE);
	ssd1306_displayPages(0, SSD1306_PAGES - 1);

	while (1) { }
}
