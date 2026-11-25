#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch32fun.h"
#include "../../../../Mess-libs/i2c/lib_i2c.h"

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

static void test_polys(void) {
    int y = 0;

    //! rectangles
    for (int8_t i = 0; i<4; i++) {
		uint8_t should_fill = i > 1 ? 1 : 0;
		ssd1306_render_rect((T_Point){ 84, y }, 15, 5, should_fill);
		y += 7;
	}

    //! zigzag
    T_Point zigzag[] = {
        (T_Point) { 60, 8 },
        (T_Point) { 50, 15 },
        (T_Point) { 80, 8 },
        (T_Point) { 70, 0 },
        (T_Point) { 70, 20 }
    };

    uint8_t pt_count = sizeof(zigzag)/sizeof(T_Point);
    ssd1306_render_solid_poly(zigzag, pt_count);

    T_Point zigzag2[5];
    memcpy(zigzag2, zigzag, sizeof(zigzag));  // Fast copy

    for (int i = 0; i < sizeof(zigzag)/sizeof(T_Point); i++) {
        zigzag2[i].y += 24;  // Add 20 to each x-coordinate
    }
    ssd1306_render_poly(zigzag2, pt_count, 1);


    // Concave polygon: Star (22px tall)
    T_Point star[] = {
        {12 , 0},  // Top point
        {16 , 8}, // Right upper
        {24 , 8}, // Right outer
        {18 , 14}, // Right inner
        {22 , 22}, // Bottom right
        {12 , 16}, // Bottom center
        {2  , 22},  // Bottom left
        {6  , 14},  // Left inner
        {0  , 8},  // Left outer
        {8  , 8}   // Left upper
    };

    // Convex polygon: Quad (12px tall)
    static T_Point quad[] = {
        {6  , 24},  // Bottom-left (aligned with star's left)
        {18 , 24}, // Bottom-right (centered)
        {22 , 34}, // Top-right (matches star width - unchanged)
        {2  , 34}   // Top-left (aligned - unchanged)
    };

    // Self-intersecting: Hourglass (12px tall)
    static T_Point hourglass[] = {
        {6  , 38},   // Top-left (aligned with quad's bottom-left)
        {18 , 38},  // Top-right (aligned with quad's bottom-right)
        {6  , 52},   // Bottom-left
        {18 , 52}   // Bottom-right
    };

    //! star
    pt_count = sizeof(star)/sizeof(T_Point);
    ssd1306_render_solid_poly(star, pt_count);

    // Shift star right by 25px
    for (int i = 0; i < sizeof(star)/sizeof(T_Point); i++) {
        star[i].x += 25;  // Add 20 to each x-coordinate
    }
    ssd1306_render_poly(star, pt_count, 1);

    //! quad
    pt_count = sizeof(quad)/sizeof(T_Point);
    ssd1306_render_solid_poly(quad, pt_count);

    // Shift quad right by 25px
    T_Point quad2[4];
    memcpy(quad2, quad, sizeof(quad));  // Fast copy

    for (int i = 0; i < sizeof(quad)/sizeof(T_Point); i++) {
        quad2[i].x += 25;  // Add 20 to each x-coordinate
    }
    ssd1306_render_poly(quad2, pt_count, 1);

    //! hourglass
    pt_count = sizeof(hourglass)/sizeof(T_Point);
    ssd1306_render_solid_poly(hourglass, pt_count);

    // Shift hourglass right by 25px
    T_Point hourglass2[4];
    memcpy(hourglass2, hourglass, sizeof(hourglass));  // Fast copy

    for (int i = 0; i < sizeof(hourglass)/sizeof(T_Point); i++) {
        hourglass2[i].x += 25;  // Add 20 to each x-coordinate
    }
    ssd1306_render_poly(hourglass2, pt_count, 1);
}


int main(void) {
	SystemInit();
	if (i2c_init(&display) != I2C_OK || i2c_ping(display.addr) != I2C_OK) {
		printf("SSD1306 not found at 0x%02X\n", display.addr);
		while (1) { }
	}

	ssd1306_setup();
	memset(frame_buffer, 0, sizeof(frame_buffer));
	test_polys();
	ssd1306_displayPages(0, SSD1306_PAGES - 1);
	while (1) { }
}
