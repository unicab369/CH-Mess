#include "ch32fun.h"
#include <stdint.h>
#include "lib/lib_tft.h"

// ST7735 Datasheet
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf


// MADCTL Parameters
#define ST7735_MADCTL_MH  0x04  // Bit 2 - Refresh Left to Right
#define ST7735_MADCTL_RGB 0x00  // Bit 3 - RGB Order
#define ST7735_MADCTL_BGR 0x08  // Bit 3 - BGR Order
#define ST7735_MADCTL_ML  0x10  // Bit 4 - Scan Address Increase
#define ST7735_MADCTL_MV  0x20  // Bit 5 - X-Y Exchange
#define ST7735_MADCTL_MX  0x40  // Bit 6 - X-Mirror
#define ST7735_MADCTL_MY  0x80  // Bit 7 - Y-Mirror


#define ST7735_CASET        0x2A    // Column Address Set
#define ST7735_RASET        0x2B    // Row Address Set
#define ST7735_RAMWR        0x2C    // RAM Write

void INTF_TFT_SET_WINDOW(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    INTF_TFT_START_WRITE();

    write_cmd_8(ST7735_CASET);
    write_data_16(x0);
    write_data_16(x1);
    write_cmd_8(ST7735_RASET);
    write_data_16(y0);
    write_data_16(y1);
    write_cmd_8(ST7735_RAMWR);
}

void INTF_TFT_SEND_BUFF(const uint8_t* buffer, uint16_t len, uint16_t repeat) {
    SPI_send_DMA(buffer, len, repeat);

    INTF_TFT_END_WRITE();
}

void INTF_TFT_SEND_COLOR(uint16_t color) {
    write_data_16(color);

    INTF_TFT_END_WRITE();
}

#define ST7735_SWRESET      0x01
#define ST7735_SLEEPON      0x10    // Sleep ON
#define ST7735_SLEEPOFF     0x11    // Sleep OFF
#define ST7735_COLMODE      0x3A    // Color Mode bit/pixel

#define ST7735_INVERTON     0x21    // Invert ON
#define ST7735_INVERTOFF    0x20    // Invert OFF
#define ST7735_NORON        0x13    // Normal Display ON

#define ST7735_DISPON       0x29    // Display ON
#define ST7735_DISPOFF      0x28    // Display OFF

#define ST7735_GAMCTRP     0xE0    // Gamma Control Positive
#define ST7735_GAMCTRN     0xE1    // Gamma Control Neigative

void fun_st7335_init() {
    INTF_TFT_START_WRITE();

    //# Software reset
    write_cmd_8(ST7735_SWRESET);
    Delay_Ms(200);
    write_cmd_8(ST7735_SLEEPOFF);
    Delay_Ms(100);

    //# Interface Pixel Format
    write_cmd_8(ST7735_COLMODE);
    write_data_8(0x05);             // 0x03: 12-bit, 0x05: 16-bit, 0x06: 18-bit, 0x07: Not used

    //# Display inversion
    write_cmd_8(ST7735_INVERTON);

    //# Normal display on
    write_cmd_8(ST7735_NORON);
    Delay_Ms(10);

    // Set rotation
    write_cmd_8(0x36);                          //# MADCTL - Memory Access Control
    // write_data_8(0x68);                 // For 1.8"
    write_data_8(ST7735_MADCTL_MY | ST7735_MADCTL_MV | ST7735_MADCTL_BGR);  // 0 - Horizontal
    // write_data_8(ST7735_MADCTL_BGR);                                        // 1 - Vertical
    // write_data_8(ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR);  // 2 - Horizontal
    // write_data_8(ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_BGR);  // 3 - Vertical
    
    //# Gamma+ Adjustments (magic numbers)
    uint8_t gamma_pos[] = {
        0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19, 0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E
    };
    write_cmd_8(ST7735_GAMCTRP);
    INTF_TFT_SEND_BUFF(gamma_pos, 16, 1);

    //# Gamma- Adjustments (magic numbers)
    uint8_t gamma_neg[] = {
        0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E, 0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F
    };
    write_cmd_8(ST7735_GAMCTRN);
    INTF_TFT_SEND_BUFF(gamma_neg, 16, 1);
    Delay_Ms(10);

    //# Display On
    write_cmd_8(ST7735_DISPON);
    Delay_Ms(10);

    INTF_TFT_END_WRITE();

    tft_fill_rect(0, 0, 160, 128, PURPLE);
}

/* White Noise Generator State */
#define NOISE_BITS 8
#define NOISE_MASK ((1<<NOISE_BITS)-1)
#define NOISE_POLY_TAP0 31
#define NOISE_POLY_TAP1 21
#define NOISE_POLY_TAP2 1
#define NOISE_POLY_TAP3 0
uint32_t lfsr = 1;

/*
 * random byte generator
 */
uint8_t rand8(void) {
    uint8_t bit;
    uint32_t new_data;

    for(bit=0;bit<NOISE_BITS;bit++) {
        new_data = ((lfsr>>NOISE_POLY_TAP0) ^
                                (lfsr>>NOISE_POLY_TAP1) ^
                                (lfsr>>NOISE_POLY_TAP2) ^
                                (lfsr>>NOISE_POLY_TAP3));
        lfsr = (lfsr<<1) | (new_data&1);
    }

    return lfsr&NOISE_MASK;
}

void fun_st7735_test2() {
    tft_set_cursor(0, 0);
    tft_print("Hello World!");
    
    static test_counter;

    //! dots test
    tft_fill_rect(0, 0, 160, 128, PURPLE);
    test_counter = 40000;

    while (test_counter-- > 0) {
        tft_draw_pixel(rand8() % 160, rand8() % 80, colors[rand8() % 19]);
    }
    
    //! draw vertical lines
    test_counter = 70;

    while (test_counter-- > 0) {
        for (int i = 0; i < 160; i++) {
            tft_draw_line(i, 0, i, 80, colors[rand8() % 19], 1);
        }
    }

    //! draw horizontal lines
    test_counter = 70;

    while(test_counter-- > 0) {
        for (int i = 0; i < 180; i++) {
            tft_draw_line(0, i, 180, i, colors[rand8() % 19], 1);
        }
    }

    //! draw random lines
    tft_fill_rect(0, 0, 160, 128, PURPLE);
    test_counter = 2000;

    while (test_counter-- > 0) {
        tft_draw_line(rand8() % 160, rand8() % 80, rand8() % 160, rand8() % 80, colors[rand8() % 19], 1);
    }
    

    //! draw poly
    int16_t triangle_x[] = {10, 40, 80};
    int16_t triangle_y[] = {20, 60, 70};

    // _draw_poly(triangle_x, triangle_y, 3, RED, 3);

    // int16_t square_x[] = {10, 60, 60, 10};
    // int16_t square_y[] = {10, 10, 60, 60};
    // _draw_poly(square_x, square_y, 4, RED, 3);

    Point16_t triangle[] = {{10, 20}, {40, 60}, {80, 70}};
    // tft_draw_poly2(triangle, 3, RED, 3);

    tft_draw_solid_poly2(triangle, 3, RED, WHITE, 2);

    // Point16_t square[] = {{10, 10}, {60, 10}, {60, 60}, {10, 60}};
    // _draw_poly2(square, 4, RED, 3);

    // tft_draw_circle((Point16_t){ 50, 50 }, 20, 0x07E0); // Green circle with radius = 30
    // tft_draw_circle((Point16_t){ 30, 30 }, 30, 0x001F); // Blue circle with radius = 40

    // tft_draw_filled_circle((Point16_t){ 50, 50 }, 10, 0x07E0);
    // tft_draw_ring((Point16_t){ 50, 50 }, 20, 0x07E0, 5); // Green ring with radius = 30 and width = 5


    //! draw rectangles
    test_counter = 100;

    while (test_counter-- > 0) {
        for (uint8_t i = 0; i < 40; i++) {
            tft_draw_rect(i, i, 160 - (i << 1), 80 - (i << 1), colors[rand8() % 19]);
        }
    }


    //! draw random rectangles
    tft_fill_rect(0, 0, 160, 128, PURPLE);
    test_counter = 10000;

    while (test_counter-- > 0) {
        tft_draw_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);
    }
    
    //! draw filled rectangles
    tft_fill_rect(0, 0, 160, 128, PURPLE);
    test_counter = 10000;

    while (test_counter-- > 0) {
        tft_fill_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);
    }

    tft_fill_rect(0, 0, 160, 128, PURPLE);

    test_counter     = 500;
    uint8_t x = 0, y = 0, step_x = 1, step_y = 1;
    while (test_counter-- > 0) {
        uint16_t bg = colors[rand8() % 19];
        tft_fill_rect(x, y, 88, 17, bg);

        Delay_Ms(25);
        x += step_x;
        if (x >= 72) step_x = -step_x;
        y += step_y;
        if (y >= 63) step_y = -step_y;
    }
}
