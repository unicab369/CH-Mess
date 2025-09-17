#include "ch32fun.h"
#include <stdint.h>
#include "lib/lib_tft.h"
#include "lib/lib_spi.h"
#include "epd_luts.h"

#include "lib/font6x8.h"
#include "../modules/fun_print.h"


#define EPD_WIDTH 128            // max 122 for 2.9" display
#define EPD_HEIGHT 250           // max 250  for 2.9" display

#define EPD_FULL_UPDATE_DATA         0xF7
#define EPD_PARTIAL_UPDATE_DATA      0xFF

static uint8_t EPD_DC_PIN, EPD_BUSY_PIN, EPD_RST_PIN;

//! REQUIRED for SPI
void FN_SPI_DC_LOW()    { funDigitalWrite(EPD_DC_PIN, 0); }
void FN_SPI_DC_HIGH()   { funDigitalWrite(EPD_DC_PIN, 1); }

void FN_EPD_WAIT_BUSY() {
    while(1) {
        if (funDigitalRead(EPD_BUSY_PIN) == 0) break;
    }
}


void fun_epd_setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    SPI_cmd_8(0x44);
    SPI_cmd_data_8((x0 >> 3) &0xFF);
    SPI_cmd_data_8(x1);

    SPI_cmd_8(0x45);
    SPI_cmd_data_8(y0);
    SPI_cmd_data_8(0x00);
    SPI_cmd_data_8(y1);
    SPI_cmd_data_8(0x00);

    Delay_Ms(100);
}

void fun_epd_setCursor(uint8_t x, uint16_t y) {
    SPI_cmd_8(0x4E);
    SPI_cmd_data_8((x >> 3) &0xFF);

    SPI_cmd_8(0x4F);
    SPI_cmd_data_8(y);
    SPI_cmd_data_8(0x00);

    Delay_Ms(100);
}

void fun_epd_update(uint8_t data) {
    SPI_cmd_8(0x22);          // Display Update Control
    SPI_cmd_data_8(data);         // 0xF7 for full update, 0xFF for partial update
    SPI_cmd_8(0x20);          // Activate Display Update Sequence
}

void fun_epd_setMode(uint8_t mode) {
    SPI_cmd_8(0x11);

    switch (mode) {
        case 0:
            //# X decrement, Y decrement
            SPI_cmd_data_8(0b0100);
            fun_epd_setWindow(EPD_WIDTH-1, EPD_HEIGHT-1, 0x00, 0x00);
            fun_epd_setCursor(EPD_WIDTH-1, EPD_HEIGHT-1);
            break;
        
        case 1:
            // # X increment, Y decrement
            SPI_cmd_data_8(0b0101);
            fun_epd_setWindow(0x00, EPD_HEIGHT-1, EPD_WIDTH-1, 0x00);
            fun_epd_setCursor(0, EPD_HEIGHT-1);
            break;

        case 2:
            //# X decrement, Y increment
            SPI_cmd_data_8(0b0110);
            fun_epd_setWindow(EPD_WIDTH-1, 0x00, 0x00, EPD_HEIGHT-1);
            fun_epd_setCursor(EPD_WIDTH-1, 0x00);
            break;

        case 3:
            //# X increment, Y increment
            SPI_cmd_data_8(0b0111);
            fun_epd_setWindow(0x00, 0x00, EPD_WIDTH-1, EPD_HEIGHT-1);
            fun_epd_setCursor(0x00, 0x00);
            break;
    }

}

void fun_epd_setLut(const uint8_t* lut) {
    SPI_cmd_8(0x32);
    
    for (uint8_t i = 0; i < 153; i++) {
        SPI_cmd_data_8(lut[i]);
    }

    SPI_cmd_8(0x3F);
    SPI_cmd_data_8(lut[153]);
    SPI_cmd_8(0x03);
    SPI_cmd_data_8(lut[154]);
    SPI_cmd_8(0x04);
    SPI_cmd_data_8(lut[155]);
    SPI_cmd_data_8(lut[156]);
    SPI_cmd_data_8(lut[157]);
    SPI_cmd_8(0x2C);
    SPI_cmd_data_8(lut[158]);
}

void fun_epd_setLutDefault(const uint8_t* lut, uint8_t size) {
    SPI_cmd_8(0x32);
    
    for (uint8_t i = 0; i < size; i++) {
        SPI_cmd_data_8(lut[i]);
    }
}

void fun_epd_reset() {
    // Reset display
    funDigitalWrite(EPD_RST_PIN, 0);
    Delay_Ms(100);                           // working with 2ms?
    funDigitalWrite(EPD_RST_PIN, 1);
    Delay_Ms(100);                           // working with 2ms?

    //# Reset Display
    SPI_cmd_8(0x12);
    Delay_Ms(100);                           // working with 2ms?

    //# Init Code
    SPI_cmd_8(0x01);
    SPI_cmd_data_8(EPD_HEIGHT-1);
    SPI_cmd_data_8(0x00);
    SPI_cmd_data_8(0b000);

    //# Border Waveform
    SPI_cmd_8(0x3C);
    SPI_cmd_data_8(0x05);

    // //# Display Update Control
    // write_cmd_8(0x21);
    // write_data_8(0x00);
    // write_data_8(0x80);

    fun_epd_setMode(1);
    Delay_Ms(100);

    // fun_epd_setLut(MyFastFullLUT);
}

void fun_epd_setup(uint8_t dc_pin, uint8_t rst_pin, uint8_t busy_pin) {
    EPD_DC_PIN = dc_pin;
    EPD_BUSY_PIN = busy_pin;
    EPD_RST_PIN = rst_pin;

    funPinMode(rst_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funPinMode(dc_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    
    if (busy_pin != 0xFF) {
        funPinMode(busy_pin, GPIO_CFGLR_IN_PUPD);
    }

    fun_epd_reset();
}

uint8_t double_bits_forHalfByte(uint8_t byte, uint8_t firstHalf) {
    uint8_t output = 0;
    uint8_t start = firstHalf ? 4 : 0;
    uint8_t end = firstHalf ? 7 : 3;

    for (int i = end; i >= start; i--) {
        uint8_t bit = (byte >> i) & 1;
        uint8_t doubled = bit ? 0b11 : 0b00;
        output = (output << 2) | doubled;
    }
    
    return output;
}

void get_font_char(char c, uint8_t width, const char *font, char* buff) {
    const char* start = &font[(c-32)*width];

    for (uint8_t i = 0; i < width; i++) {
        buff[i] = *start++;
    }
}

void render_string_6x8(const char* str, uint8_t vertical, uint8_t horizontal) {
    char verLINES = 6;
    char char_buff[8] = {0};
    fun_epd_setCursor(vertical, horizontal);         // set cursor
    SPI_cmd_8(0x24);                                  // write to RAM command - Black/White

    for (int i = 0; i<strlen(str); i++) {
        get_font_char(str[i], verLINES, &font6x8, char_buff);

        for (int j = 0; j < verLINES; j++) {
            char target = ~(char_buff[j]);
            SPI_cmd_data_8(target);            
        }
    }

    fun_epd_update(EPD_FULL_UPDATE_DATA);                            // full update sequence
}


void render_str_paging(
    const char* str, uint8_t partCount, uint8_t vertical, uint8_t horizontal,
    char (*handle_LinePaging)(uint8_t page, char lineBits)
) {
    // fun_epd_reset();
    // Delay_Ms(100);
    // // fun_edp_setLut(MyPartialLUT);
    // fun_epd_setLutDefault(lut_partial3, sizeof(lut_partial3));
    // Delay_Ms(100);
    
    char verLINES = 6;
    char char_buff[8] = {0};
    uint8_t ref_horz, ref_vert;

    for (int p=0; p < partCount; p++) {
        ref_horz = horizontal;
        ref_vert = vertical + p*8;
        fun_epd_setCursor(ref_vert, ref_horz);       // set cursor
        SPI_cmd_8(0x24);                              // write to RAM command - Black/White

        for (int i = 0; i < strlen(str); i++) {
            get_font_char(str[i], verLINES, &font6x8, char_buff); 

            if (ref_horz < verLINES) {
                // # update cursor for next line
                ref_horz = EPD_HEIGHT-1;
                ref_vert = ref_vert + 8*partCount;
                fun_epd_setCursor(ref_vert, ref_horz);
                SPI_cmd_8(0x24);
            }

            for (int j = 0; j < verLINES; j++) {
                char lineBits = ~(char_buff[j]);
                char half = handle_LinePaging(p, lineBits);
                SPI_cmd_data_8(half);
                // write_data_8(halfTop);
            }

            ref_horz = ref_horz - verLINES;
        }
    }

    fun_epd_update(EPD_FULL_UPDATE_DATA);
}

char handle_12x8_linePaging(uint8_t linePage, char lineBits) {
    switch (linePage) {
        case 0: return double_bits_forHalfByte(lineBits, 1);
        case 1: return double_bits_forHalfByte(lineBits, 0);
    }
}

char handle_24x8_linePaging(uint8_t linePage, char lineBits) {
    char topHalf = double_bits_forHalfByte(lineBits, 1);
    char bottomHalf = double_bits_forHalfByte(lineBits, 0);

    switch (linePage) {
        case 0: return double_bits_forHalfByte(topHalf, 1);
        case 1: return double_bits_forHalfByte(topHalf, 0);
        case 2: return double_bits_forHalfByte(bottomHalf, 1);
        case 3: return double_bits_forHalfByte(bottomHalf, 0);
    }
}


void render_string_12x8(const char* str, uint8_t vertical, uint8_t horizontal) {
    printf("str len: %lu\n", strlen(str));
    render_str_paging(str, 2, vertical, horizontal, handle_12x8_linePaging);
}


void fun_epd_fill(uint8_t byte, uint8_t vertical, uint8_t horizontal) {
    fun_epd_setCursor(vertical, horizontal);
    SPI_cmd_8(0x26);
    
    for (int i = 0; i < (EPD_WIDTH >> 3) * EPD_HEIGHT; i++) {
        SPI_cmd_data_8(0x00);
    }

    fun_epd_setCursor(vertical, horizontal);
    SPI_cmd_8(0x24);

    for (int i = 0; i < (EPD_WIDTH >> 3) * EPD_HEIGHT; i++) {
        SPI_cmd_data_8(byte);
    }

    fun_epd_update(EPD_FULL_UPDATE_DATA);
}

void fun_epd_fillLen(uint8_t byte, uint16_t len) {
    SPI_cmd_8(0x24);

    for (int i = 0; i < len; i++) {
        SPI_cmd_data_8(byte);
    }

    fun_epd_update(EPD_FULL_UPDATE_DATA);

    while(1) {
        if (funDigitalRead(PC7) == 0) break;
    }
}