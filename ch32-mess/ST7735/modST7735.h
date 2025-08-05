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

#include "modSpi.h"
#include "font5x7.h"


// Define screen resolution and offset
#define ST7735_WIDTH    160
#define ST7735_HEIGHT   80
#define ST7735_X_OFFSET 1
#define ST7735_Y_OFFSET 26

// Note: To not use CS, uncomment the following line and pull CS to ground.
//  #define ST7735_NO_CS

#define RGB565(r, g, b) ((((r)&0xF8) << 8) | (((g)&0xFC) << 3) | ((b) >> 3))
#define BGR565(r, g, b) ((((b)&0xF8) << 8) | (((g)&0xFC) << 3) | ((r) >> 3))
#define RGB             RGB565

#define BLACK       RGB(0, 0, 0)
#define NAVY        RGB(0, 0, 123)
#define DARKGREEN   RGB(0, 125, 0)
#define DARKCYAN    RGB(0, 125, 123)
#define MAROON      RGB(123, 0, 0)
#define PURPLE      RGB(123, 0, 123)
#define OLIVE       RGB(123, 125, 0)
#define LIGHTGREY   RGB(198, 195, 198)
#define DARKGREY    RGB(123, 125, 123)
#define BLUE        RGB(0, 0, 255)
#define GREEN       RGB(0, 255, 0)
#define CYAN        RGB(0, 255, 255)
#define RED         RGB(255, 0, 0)
#define MAGENTA     RGB(255, 0, 255)
#define YELLOW      RGB(255, 255, 0)
#define WHITE       RGB(255, 255, 255)
#define ORANGE      RGB(255, 165, 0)
#define GREENYELLOW RGB(173, 255, 41)
#define PINK        RGB(255, 130, 198)


static uint16_t colors[] = {
    BLACK, NAVY, DARKGREEN, DARKCYAN, MAROON, PURPLE, OLIVE,  LIGHTGREY,   DARKGREY, BLUE,
    GREEN, CYAN, RED,       MAGENTA,  YELLOW, WHITE,  ORANGE, GREENYELLOW, PINK,
};

uint8_t rand8(void);


// #ifndef ST7735_NO_CS
//     #define PIN_CS 4  // PC4
// #endif
#define SPI_SCLK 5  // PC5
#define SPI_MOSI 6  // PC6

// #define DATA_MODE()    (GPIOC->BSHR |= 1 << PIN_DC)  // DC High

#ifndef ST7735_NO_CS
    #define START_WRITE() (GPIOC->BCR |= 1 << PIN_CS)   // CS Low
    #define END_WRITE()   (GPIOC->BSHR |= 1 << PIN_CS)  // CS High
#else
    #define START_WRITE()
    #define END_WRITE()
#endif


// ST7735 Datasheet
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
// Delays

// System Function Command List - Write Commands Only
#define ST7735_SLPIN   0x10  // Sleep IN
#define ST7735_SLPOUT  0x11  // Sleep Out
#define ST7735_PTLON   0x12  // Partial Display Mode On
#define ST7735_NORON   0x13  // Normal Display Mode On
#define ST7735_INVOFF  0x20  // Display Inversion Off
#define ST7735_INVON   0x21  // Display Inversion On
#define ST7735_GAMSET  0x26  // Gamma Set
#define ST7735_DISPOFF 0x28  // Display Off
#define ST7735_DISPON  0x29  // Display On
#define ST7735_CASET   0x2A  // Column Address Set
#define ST7735_RASET   0x2B  // Row Address Set
#define ST7735_RAMWR   0x2C  // Memory Write
#define ST7735_PLTAR   0x30  // Partial Area
#define ST7735_TEOFF   0x34  // Tearing Effect Line Off
#define ST7735_TEON    0x35  // Tearing Effect Line On
#define ST7735_MADCTL  0x36  // Memory Data Access Control
#define ST7735_IDMOFF  0x38  // Idle Mode Off
#define ST7735_IDMON   0x39  // Idle Mode On
#define ST7735_COLMOD  0x3A  // Interface Pixel Format

// Panel Function Command List - Only Used
#define ST7735_GMCTRP1 0xE0  // Gamma '+' polarity Correction Characteristics Setting
#define ST7735_GMCTRN1 0xE1  // Gamma '-' polarity Correction Characteristics Setting

// MADCTL Parameters
#define ST7735_MADCTL_MH  0x04  // Bit 2 - Refresh Left to Right
#define ST7735_MADCTL_RGB 0x00  // Bit 3 - RGB Order
#define ST7735_MADCTL_BGR 0x08  // Bit 3 - BGR Order
#define ST7735_MADCTL_ML  0x10  // Bit 4 - Scan Address Increase
#define ST7735_MADCTL_MV  0x20  // Bit 5 - X-Y Exchange
#define ST7735_MADCTL_MX  0x40  // Bit 6 - X-Mirror
#define ST7735_MADCTL_MY  0x80  // Bit 7 - Y-Mirror

// COLMOD Parameter
#define ST7735_COLMOD_16_BPP 0x05  // 101 - 16-bit/pixel

// 5x7 Font
#define FONT_WIDTH  5  // Font width
#define FONT_HEIGHT 7  // Font height

static uint16_t _cursor_x                  = 0;
static uint16_t _cursor_y                  = 0;      // Cursor position (x, y)
// static uint16_t _color                     = WHITE;  // Color
static uint16_t _bg_color                  = BLACK;  // Background color
static uint8_t  _buffer[ST7735_WIDTH << 1] = {0};    // DMA buffer, long enough to fill a row.
static uint8_t DC_PIN2;

void tft_send_DMA(const uint8_t* buffer, uint16_t size, uint16_t repeat) {
    SPI_send_DMA(DC_PIN2, buffer, size, repeat);
}

/// \brief Initialize ST7735
/// \details Initialization sequence from Arduino_GFX
/// https://github.com/moononournation/Arduino_GFX/blob/master/src/display/Arduino_ST7735.h
void tft_init(uint8_t rst_pin, uint8_t dc_pin) {
    DC_PIN2 = dc_pin;

    funPinMode(rst_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funPinMode(dc_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);

    // Reset display
    funDigitalWrite(rst_pin, 0);
    Delay_Ms(100);
    funDigitalWrite(rst_pin, 1);
    Delay_Ms(100);

    START_WRITE();

    write_command_8(DC_PIN2, 0x01);          // Software reset
    Delay_Ms(200);
    write_command_8(DC_PIN2, ST7735_SLPOUT);
    Delay_Ms(100);

    // Set rotation
    write_command_8(DC_PIN2, ST7735_MADCTL);
    write_data_8(DC_PIN2, ST7735_MADCTL_MY | ST7735_MADCTL_MV | ST7735_MADCTL_BGR);  // 0 - Horizontal
    // write_data_8(ST7735_MADCTL_BGR);                                        // 1 - Vertical
    // write_data_8(ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR);  // 2 - Horizontal
    // write_data_8(ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_BGR);  // 3 - Vertical

    // Set Interface Pixel Format - 16-bit/pixel
    write_command_8(DC_PIN2, ST7735_COLMOD);
    write_data_8(DC_PIN2, ST7735_COLMOD_16_BPP);

    // Gamma Adjustments (pos. polarity), 16 args.
    // (Not entirely necessary, but provides accurate colors)
    uint8_t gamma_p[] = {0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19,
                        0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E};
    write_command_8(DC_PIN2, ST7735_GMCTRP1);
    tft_send_DMA(gamma_p, 16, 1);

    // Gamma Adjustments (neg. polarity), 16 args.
    // (Not entirely necessary, but provides accurate colors)
    uint8_t gamma_n[] = {0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E,
                        0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F};
    write_command_8(DC_PIN2, ST7735_GMCTRN1);
    tft_send_DMA(gamma_n, 16, 1);

    Delay_Ms(10);

    write_command_8(DC_PIN2, 0x26);  //! Gamma disable

    // Invert display
    write_command_8(DC_PIN2, ST7735_INVON);
    // write_command_8(ST7735_INVOFF);

    // Normal display on, no args, w/delay
    write_command_8(DC_PIN2, ST7735_NORON);
    Delay_Ms(10);

    // Main screen turn on, no args, w/delay
    write_command_8(DC_PIN2, ST7735_DISPON);
    Delay_Ms(10);

    END_WRITE();
}

void tft_set_cursor(uint16_t x, uint16_t y) {
    _cursor_x = x + ST7735_X_OFFSET;
    _cursor_y = y + ST7735_Y_OFFSET;
}


// void tft_set_color(uint16_t color) {
//     _color = color;
// }


void tft_set_background_color(uint16_t color) {
    _bg_color = color;
}

static void tft_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    write_command_8(DC_PIN2, ST7735_CASET);
    write_data_16(DC_PIN2, x0);
    write_data_16(DC_PIN2, x1);
    write_command_8(DC_PIN2, ST7735_RASET);
    write_data_16(DC_PIN2, y0);
    write_data_16(DC_PIN2, y1);
    write_command_8(DC_PIN2, ST7735_RAMWR);
}

void tft_send_color(uint16_t color) {
    write_data_16(DC_PIN2, color);
}