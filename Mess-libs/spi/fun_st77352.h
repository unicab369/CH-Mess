// use some code from https://github.com/moononournation/Arduino_GFX

#include "ch32fun.h"
#include <stdint.h>
#include "lib/lib_tft.h"
#include "spi_port.h"

// ST7735 Datasheet
// https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf
#define ST7735_RGB(r, g, b) ((r >> 3) | ((g & 0xFC) << 3) | ((b & 0xF8) << 8))

#define ST_BLACK       ST7735_RGB(0     , 0     , 0)
#define ST_WHITE       ST7735_RGB(0xFF  , 0xFF  , 0xFF)
#define ST_RED         ST7735_RGB(0xFF  , 0     , 0)
#define ST_GREEN       ST7735_RGB(0     , 0xFF  , 0)
#define ST_BLUE        ST7735_RGB(0     , 0     , 0xFF)
#define ST_PURPLE      ST7735_RGB(0x80  , 0     , 0x80)
#define ST_YELLOW      ST7735_RGB(0xFF  , 0xFF  , 0)
#define ST_CYAN        ST7735_RGB(0     , 0xFF  , 0xFF)
#define ST_MAGENTA     ST7735_RGB(0xFF  , 0     , 0xFF)
#define ST_ORANGE      ST7735_RGB(0xFF  , 0x80  , 0)

//###########################################
//# INTERFACES
//###########################################

uint32_t ST7735_CS_PIN = SPI_PORT_PIN_NONE;

// Disable
void TFT_CS_HIGH() {
    if (ST7735_CS_PIN == SPI_PORT_PIN_NONE) return;
    funDigitalWrite(ST7735_CS_PIN, 1);
}

// Enable
void TFT_CS_LOW() {
    if (ST7735_CS_PIN == SPI_PORT_PIN_NONE) return;
    funDigitalWrite(ST7735_CS_PIN, 0);
}

#define ST7735_CASET        0x2A    // Column Address Set
#define ST7735_RASET        0x2B    // Row Address Set
#define ST7735_RAMWR        0x2C    // RAM Write

uint8_t ST7735_XOFFSET = 0;
uint8_t ST7735_YOFFSET = 0;

void TFT_SET_WINDOW(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    SPI_CMD_8(ST7735_CASET);
    SPI_CMD_DATA_16(x0 + ST7735_XOFFSET);
    SPI_CMD_DATA_16(x1 + ST7735_XOFFSET);
    SPI_CMD_8(ST7735_RASET);
    SPI_CMD_DATA_16(y0 + ST7735_YOFFSET);
    SPI_CMD_DATA_16(y1 + ST7735_YOFFSET);
    SPI_CMD_8(ST7735_RAMWR);
}

void TFT_SEND_BUFF8(const uint8_t* buffer, uint16_t len) {
    for (int i = 0; i < len; i++) SPI_CMD_DATA_8(buffer[i]);
}

void TFT_SEND_BUFF16(const uint16_t* buffer, uint16_t len) {
    for (int i = 0; i < len; i++) SPI_CMD_DATA_16(buffer[i]);
}

void TFT_SEND_PIXEL(uint16_t color) {
    SPI_CMD_DATA_16(color);
}


//###########################################
//# CONFIGURATIONS
//###########################################

#define ST7735_SWRESET      0x01
#define ST7735_SLEEPON      0x10    // Sleep ON
#define ST7735_SLEEPOFF     0x11    // Sleep OFF
#define ST7735_COLMODE      0x3A    // Color Mode bit/pixel

#define ST7735_INVERTON     0x21    // Invert ON
#define ST7735_INVERTOFF    0x20    // Invert OFF
#define ST7735_NORON        0x13    // Normal Display ON
#define ST7735_MADCTL       0x36    // Memory Access Control

#define ST7735_DISPON       0x29    // Display ON
#define ST7735_DISPOFF      0x28    // Display OFF
#define ST7735_GAMCTRP      0xE0    // Gamma Control Positive
#define ST7735_GAMCTRN      0xE1    // Gamma Control Neigative

uint8_t ST7735_WIDTH = 160;
uint8_t ST7735_HEIGHT = 80;

void st7735_fill(uint16_t color) {
    tft_fill_rect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void st7735_init(uint8_t width, uint8_t height, uint32_t cs_pin) {
    ST7735_WIDTH = width;
    ST7735_HEIGHT = height;

    // different screen sizes have different offsets
    if (height == 80) {
        ST7735_XOFFSET = 1;
        ST7735_YOFFSET = 26;
    }

    if (cs_pin != SPI_PORT_PIN_NONE) {
        ST7735_CS_PIN = cs_pin;
#if defined(CH582_CH583) || defined(CH571_CH573)
        funPinMode(cs_pin, GPIO_ModeOut_PP_5mA);
#else
        funPinMode(cs_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
#endif
        funDigitalWrite(cs_pin, 1);
    }

    TFT_CS_LOW();

    //# Software reset
    SPI_CMD_8(ST7735_SWRESET);
    Delay_Ms(110);                      //! 1.8" display need at least 110ms
    SPI_CMD_8(ST7735_SLEEPOFF);
    Delay_Ms(20);

    //# Interface Pixel Format
    SPI_CMD_8(ST7735_COLMODE);
    SPI_CMD_DATA_8(0x05);               // 0x03: 12-bit, 0x05: 16-bit, 0x06: 18-bit, 0x07: Not used

    //# Display inversion
    SPI_CMD_8(ST7735_INVERTON);

    //# Normal display on
    SPI_CMD_8(ST7735_NORON);

    //# MADCTL - Memory Access Control
    uint8_t MADCTL_MY = 0b10000000;     // bit7: Row address order
    uint8_t MADCTL_MX = 0b01000000;     // bit6: Column address order
    uint8_t MADCTL_MV = 0b00100000;     // bit5: Row/Column exchange
    uint8_t MADCTL_ML = 0b00010000;     // bit4: Vertical refresh order (0 = top to bottom, 1 = bottom to top)
    uint8_t MADCTL_MH = 0b00000100;     // bit2: Horizontal refresh order (0 = left to right, 1 = right to left)
    uint8_t MADCTL_RGB = 0b00001000;    // bit3: Color order (0 = RGB, 1 = BGR)

    // & 0xFF to turn ON, & 0x00 to turn OFF
    SPI_CMD_8(ST7735_MADCTL);
    uint8_t ctrValue = (MADCTL_MY & 0xFF) | (MADCTL_MV & 0xFF);
    SPI_CMD_DATA_8(ctrValue);

    //# Gamma+ Adjustments Control (magic numbers)
    uint8_t gamma_pos[] = {
        0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19,
        0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E
    };
    SPI_CMD_8(ST7735_GAMCTRP);
    TFT_SEND_BUFF8(gamma_pos, 16);

    //# Gamma- Adjustments Control (magic numbers)
    uint8_t gamma_neg[] = {
        0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E,
        0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F
    };
    SPI_CMD_8(ST7735_GAMCTRN);
    TFT_SEND_BUFF8(gamma_neg, 16);
    // Delay_Ms(10);

    //# Display On
    SPI_CMD_8(ST7735_DISPON);
    // Delay_Ms(100);

    TFT_CS_HIGH();
}
