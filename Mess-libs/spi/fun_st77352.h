#include "ch32fun.h"
#include <stdint.h>
#include "lib/lib_tft.h"

// ST7735 Datasheet
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
#define ST7735_RGB(r, g, b) ((r >> 3) | ((g&0xFC) << 3) | ((b&0xF8) << 8))

#define BLACK       ST7735_RGB(0, 0, 0)
#define WHITE       ST7735_RGB(0xFF, 0xFF, 0xFF)
#define RED         ST7735_RGB(0xFF , 0     , 0)
#define GREEN       ST7735_RGB(0    , 0xFF  , 0)
#define BLUE        ST7735_RGB(0    , 0     , 0xFF)
#define PURPLE      ST7735_RGB(0x80 , 0     , 0x80)
#define YELLOW      ST7735_RGB(0xFF , 0xFF  , 0)
#define CYAN        ST7735_RGB(0    , 0xFF  , 0xFF)
#define MAGENTA     ST7735_RGB(0xFF , 0     , 0xFF)
#define ORANGE      ST7735_RGB(0xFF , 0x80  , 0)


static uint16_t st7735_colors[] = {
    RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE
};

#define ST7735_CASET        0x2A    // Column Address Set
#define ST7735_RASET        0x2B    // Row Address Set
#define ST7735_RAMWR        0x2C    // RAM Write

uint8_t ST7735_CS_PIN = -1;

// Disable
void INT_TFT_CS_HIGH() {
    if (ST7735_CS_PIN == -1) return;
    funDigitalWrite(ST7735_CS_PIN, 1);
}

// Enable
void INT_TFT_CS_LOW() {
    if (ST7735_CS_PIN == -1) return;
    funDigitalWrite(ST7735_CS_PIN, 0);
}

void INTF_TFT_SET_WINDOW(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
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
}

void INTF_TFT_SEND_PIXEL(uint16_t x0, uint16_t y0, uint16_t color) {
    INTF_TFT_SET_WINDOW(x0, y0, x0, y0);
    write_data_16(color);
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

#define ST7735_GAMCTRP      0xE0    // Gamma Control Positive
#define ST7735_GAMCTRN      0xE1    // Gamma Control Neigative

#define ST7735_MADCTL       0x36    // Memory Access Control
#define ST7735_MADCTL_ML    0x10  // Bit 4 - Scan Address Increase
#define ST7735_MADCTL_MV    0x20  // Bit 5 - X-Y Exchange

uint8_t ST7735_WIDTH = 160;
uint8_t ST7735_HEIGHT = 80;

void fun_st7335_init(uint8_t width, uint8_t height, uint8_t cs_pin) {
    ST7735_WIDTH = width;
    ST7735_HEIGHT = height;

    if (cs_pin != -1) {
        ST7735_CS_PIN = cs_pin;
        funPinMode(cs_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
        funDigitalWrite(cs_pin, 1);
    }

    INT_TFT_CS_LOW();

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

    //# MADCTL - Memory Access Control
    uint8_t MADCTL_MY = 0b10000000;     // bit7: Row address order
    uint8_t MADCTL_MX = 0b01000000;     // bit6: Column address order
    uint8_t MADCTL_MV = 0b00100000;     // bit5: Row/Column exchange
    uint8_t MADCTL_ML = 0b00010000;     // bit4: Vertical refresh order (0 = top to bottom, 1 = bottom to top)
    uint8_t MADCTL_MH = 0b00000100;     // bit2:Horizontal refresh order (0 = left to right, 1 = right to left)
    uint8_t MADCTL_RGB = 0b00001000;    // bit3: Color order (0 = RGB, 1 = BGR)

    // &0 to turn off
    write_cmd_8(ST7735_MADCTL);
    uint8_t ctrValue = (MADCTL_MY & 0xFF) | (MADCTL_MV & 0xFF);
    write_data_8(ctrValue);
    
    //# Gamma+ Adjustments Control (magic numbers)
    uint8_t gamma_pos[] = {
        0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19, 0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E
    };
    write_cmd_8(ST7735_GAMCTRP);
    INTF_TFT_SEND_BUFF(gamma_pos, 16, 1);

    //# Gamma- Adjustments Control (magic numbers)
    uint8_t gamma_neg[] = {
        0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E, 0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F
    };
    write_cmd_8(ST7735_GAMCTRN);
    INTF_TFT_SEND_BUFF(gamma_neg, 16, 1);
    Delay_Ms(10);

    //# Display On
    write_cmd_8(ST7735_DISPON);
    Delay_Ms(10);

    INT_TFT_CS_HIGH();

    tft_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, PURPLE);
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

void fun_st7735_test() {
    tft_set_cursor(0, 0);
    tft_print("Hello World!");
    
    static int idx_counter;
    static uint8_t color_idx = 0;

    uint8_t idx = color_idx++ % (sizeof(st7735_colors)/sizeof(uint16_t));
    uint16_t color = st7735_colors[idx % sizeof(st7735_colors)];

    //! draw vertical lines
    static uint8_t x_idx = 0;
    static uint8_t y_idx = 0;

    uint8_t x_value = x_idx++ % ST7735_WIDTH;
    tft_draw_line(x_value, 0, x_value, ST7735_HEIGHT, color, 1);

    //! draw horizontal lines
    uint8_t y_value = y_idx++ % ST7735_HEIGHT;
    tft_draw_line(0, y_value, ST7735_WIDTH, y_value, color, 1);

    //! draw random lines
    tft_draw_line(rand8() % 160, rand8() % 80, rand8() % 160, rand8() % 80, color, 1);

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
    tft_draw_rect(rand8() % 140, rand8() % 60, 20, 20, color);

    //! draw random rectangles
    tft_fill_rect(rand8() % 140, rand8() % 60, 20, 20, color);
}
