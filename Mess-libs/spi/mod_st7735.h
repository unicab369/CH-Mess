/// \brief ST7735 Driver for CH32V003 - Demo
///
/// \author Li Mingjie
///  - Email:  limingjie@outlook.com
///  - GitHub: https://github.com/limingjie/
///
/// \date Aug 2023
///
/// \section References
///  - https://github.com/moononournation/Arduino_GFX
///  - https://gitee.com/morita/ch32-v003/tree/master/Driver
///  - https://github.com/cnlohr/ch32v003fun/tree/master/examples/spi_oled
///
/// \copyright Attribution-NonCommercial-ShareAlike 4.0 (CC BY-NC-SA 4.0)
///  - Attribution - You must give appropriate credit, provide a link to the
///    license, and indicate if changes were made. You may do so in any
///    reasonable manner, but not in any way that suggests the licensor endorses
///    you or your use.
///  - NonCommercial - You may not use the material for commercial purposes.
///  - ShareAlike - If you remix, transform, or build upon the material, you
///    must distribute your contributions under the same license as the original.
///
/// \section Wiring
/// | CH32V003       | ST7735    | Power | Description                       |
/// | -------------- | --------- | ----- | --------------------------------- |
/// |                | 1 - LEDA  | 3V3   | Use PWM to control brightness     |
/// |                | 2 - GND   | GND   | GND                               |
/// | PC2            | 3 - RESET |       | Reset                             |
/// | PC3            | 4 - RS    |       | DC (Data / Command)               |
/// | PC6 (SPI MOSI) | 5 - SDA   |       | SPI MOSI (Master Output Slave In) |
/// | PC5 (SPI SCLK) | 6 - SCL   |       | SPI SCLK (Serial Clock)           |
/// |                | 7 - VDD   | 3V3   | VDD                               |
/// | PC4            | 8 - CS    |       | SPI CS/SS (Chip/Slave Select)     |


#include "ch32fun.h"
#include <stdint.h>
#include "lib_tft.h"

// ST7735 Datasheet
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
// Delays

// System Function Command List - Write Commands Only
#define ST7735_PTLON   0x12  // Partial Display Mode On
#define ST7735_GAMSET  0x26  // Gamma Set
#define ST7735_CASET   0x2A  // Column Address Set
#define ST7735_RASET   0x2B  // Row Address Set
#define ST7735_RAMWR   0x2C  // Memory Write
#define ST7735_PLTAR   0x30  // Partial Area
#define ST7735_TEOFF   0x34  // Tearing Effect Line Off
#define ST7735_TEON    0x35  // Tearing Effect Line On
#define ST7735_IDMOFF  0x38  // Idle Mode Off
#define ST7735_IDMON   0x39  // Idle Mode On


// MADCTL Parameters
#define ST7735_MADCTL_MH  0x04  // Bit 2 - Refresh Left to Right
#define ST7735_MADCTL_RGB 0x00  // Bit 3 - RGB Order
#define ST7735_MADCTL_BGR 0x08  // Bit 3 - BGR Order
#define ST7735_MADCTL_ML  0x10  // Bit 4 - Scan Address Increase
#define ST7735_MADCTL_MV  0x20  // Bit 5 - X-Y Exchange
#define ST7735_MADCTL_MX  0x40  // Bit 6 - X-Mirror
#define ST7735_MADCTL_MY  0x80  // Bit 7 - Y-Mirror


static uint8_t DC_PIN;

void INTF_SPI_DC_LOW()  { funDigitalWrite(DC_PIN, 0); }
void INTF_SPI_DC_HIGH() { funDigitalWrite(DC_PIN, 1); }

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

void INTF_TFT_SEND_BUFF(const uint8_t* buffer, uint16_t size, uint16_t repeat) {
    INTF_SPI_DC_HIGH();
    SPI_send_DMA(buffer, size, repeat);

    INTF_TFT_END_WRITE();
}

void INTF_TFT_SEND_COLOR(uint16_t color) {
    write_data_16(color);

    INTF_TFT_END_WRITE();
}

/// \brief Initialize ST7735
/// \details Initialization sequence from Arduino_GFX
/// https://github.com/moononournation/Arduino_GFX/blob/master/src/display/Arduino_ST7735.h
void mod_st7335_init(uint8_t rst_pin, uint8_t dc_pin) {
    DC_PIN = dc_pin;

    funPinMode(rst_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funPinMode(dc_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);

    // Reset display
    funDigitalWrite(rst_pin, 0);
    Delay_Ms(100);
    funDigitalWrite(rst_pin, 1);
    Delay_Ms(100);

    INTF_TFT_START_WRITE();

    write_cmd_8(0x01);              //# Software reset
    Delay_Ms(200);
    write_cmd_8(0x11);              //# SLPOUT - Sleep Out; SLPIN 0x10
    Delay_Ms(100);

    // Set rotation
    write_cmd_8(0x36);              //# MADCTL - Memory Access Control
    // write_data_8(0x68);                 // For 1.8"
    write_data_8(ST7735_MADCTL_MY | ST7735_MADCTL_MV | ST7735_MADCTL_BGR);  // 0 - Horizontal
    // write_data_8(ST7735_MADCTL_BGR);                                        // 1 - Vertical
    // write_data_8(ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR);  // 2 - Horizontal
    // write_data_8(ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_BGR);  // 3 - Vertical

    // Set Interface Pixel Format
    write_cmd_8(0x3A);              //# COLMOD - Color Mode
    write_data_8(0x05);             // 16-bit/pixel

    // Gamma Adjustments (pos. polarity), 16 args.
    // (Not entirely necessary, but provides accurate colors)
    uint8_t gamma_p[] = {0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19,
                        0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E};
    write_cmd_8(0xE0);              //# GMCTRP1 - Gamama Control + Positive Polarity
    INTF_TFT_SEND_BUFF(gamma_p, 16, 1);

    // Gamma Adjustments (neg. polarity), 16 args.
    // (Not entirely necessary, but provides accurate colors)
    uint8_t gamma_n[] = {0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E,
                        0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F};
    write_cmd_8(0xE1);              //# GMCTRN1 - Gamma Control - Negative Polarity
    INTF_TFT_SEND_BUFF(gamma_n, 16, 1);
    Delay_Ms(10);

    // Invert display
    write_cmd_8(0x21);              //# INVON - Inversion On
    // write_cmd_8(0x20);              //# INVOFF - Inversion Off: For 1.8"

    // Normal display on, no args, w/delay
    write_cmd_8(0x13);              //# NORON - Normal Display On
    Delay_Ms(10);

    // Main screen turn on, no args, w/delay
    write_cmd_8(0x29);              //# DISPON - Display On; DISPOFF 0x28
    Delay_Ms(10);

    INTF_TFT_END_WRITE();
}

void mod_st7735_setup(uint8_t rst_pin, uint8_t dc_pin) {
    mod_st7335_init(rst_pin, dc_pin);
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


static uint32_t frame = 0;

int mod_st7735_test1(void) {
    // tft_set_color(RED);
    // popup("Draw Point", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame = 30000;
    while (frame-- > 0)
    {
        tft_draw_pixel(rand8() % 160, rand8() % 80, colors[rand8() % 19]);
    }

    // popup("Scan Line", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame = 50;
    while (frame-- > 0)
    {
        for (uint8_t i = 0; i < 160; i++)
        {
            tft_draw_line(i, 0, i, 80, colors[rand8() % 19], 1);
        }
    }
    frame = 50;
    while (frame-- > 0)
    {
        for (uint8_t i = 0; i < 80; i++)
        {
            tft_draw_line(0, i, 180, i, colors[rand8() % 19], 1);
        }
    }

    // popup("Draw Line", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame = 2000;
    while (frame-- > 0)
    {
        tft_draw_line(rand8() % 160, rand8() % 80, rand8() % 160, rand8() % 80, colors[rand8() % 19], 1);
    }

    // popup("Scan Rect", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame = 100;
    while (frame-- > 0)
    {
        for (uint8_t i = 0; i < 40; i++)
        {
            tft_draw_rect(i, i, 160 - (i << 1), 80 - (i << 1), colors[rand8() % 19]);
        }
    }

    // popup("Draw Rect", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame = 5000;
    while (frame-- > 0)
    {
        tft_draw_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);
    }

    // popup("Fill Rect", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame = 5000;
    while (frame-- > 0)
    {
        tft_fill_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);
    }

    // popup("Move Text", 1000);
    tft_fill_rect(0, 0, 160, 80, BLACK);

    frame     = 500;
    uint8_t x = 0, y = 0, step_x = 1, step_y = 1;
    while (frame-- > 0)
    {
        uint16_t bg = colors[rand8() % 19];
        tft_fill_rect(x, y, 88, 17, bg);
        // tft_set_color(colors[rand8() % 19]);
        // tft_set_background_color(bg);
        // tft_set_cursor(x + 5, y + 5);
        // tft_print("Hello, World!");
        Delay_Ms(25);

        x += step_x;
        if (x >= 72)
        {
            step_x = -step_x;
        }
        y += step_y;
        if (y >= 63)
        {
            step_y = -step_y;
        }
    }
}


void mod_st7735_test2() {
    tft_set_cursor(0, 0);
    tft_print("Hello World!");
    tft_print_number(123456789, 0);
    
    //! dots test
    tft_draw_pixel(rand8() % 160, rand8() % 80, colors[rand8() % 19]);

    // //! draw vertical lines
    static uint8_t x_idx = 0;
    tft_draw_line(x_idx, 0, x_idx, 80, colors[rand8() % 19], 1);
    x_idx += 1;
    if (x_idx >= 160) x_idx = 0;

    // //! draw horizontal lines
    static uint8_t y_idx = 0;
    tft_draw_line(0, y_idx, 180, y_idx, colors[rand8() % 19], 1);
    y_idx += 1;
    if (y_idx >= 80) y_idx = 0;

    //! draw random lines
    tft_draw_line(0, 0, 70, 70, RED, 5);

    tft_draw_line(rand8() % 160, rand8() % 80, rand8() % 160, rand8() % 80, colors[rand8() % 19], 1);

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


    // draw rectangles
    static uint8_t rect_idx = 0;
    tft_draw_rect(rect_idx, rect_idx, 160 - (rect_idx << 1), 80 - (rect_idx << 1), colors[rand8() % 19]);
    rect_idx += 1;
    if (rect_idx >= 40) rect_idx = 0;

    // draw random rectangles
    tft_draw_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);

    // draw filled rectangles
    tft_fill_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);


    // static uint8_t x = 0, y = 0, step_x = 1, step_y = 1;

    // uint16_t bg = colors[rand8() % 19];
    // tft_fill_rect(x, y, 88, 17, bg);
    // tft_set_color(colors[rand8() % 19]);
    // tft_set_background_color(bg);
    // tft_set_cursor(x + 5, y + 5);
    // tft_print("Hello, World!");
    // // Delay_Ms(25);

    // x += step_x;
    // if (x >= 72) step_x = -step_x;
    // y += step_y;
    // if (y >= 63) step_y = -step_y;
}
