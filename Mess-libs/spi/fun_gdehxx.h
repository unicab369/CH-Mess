#include "ch32fun.h"
#include <stdint.h>
#include "lib_tft.h"
#include "lib_spi.h"

#include "font7x8.h"
#include "../modules/fun_print.h"

static uint8_t GDEH_DC_PIN;


void INTF_SPI_DC_LOW()  { funDigitalWrite(GDEH_DC_PIN, 0); }
void INTF_SPI_DC_HIGH() { funDigitalWrite(GDEH_DC_PIN, 1); }

void INTF_TFT_SEND_BUFF(const uint8_t* buffer, uint16_t size, uint16_t repeat) {
    INTF_SPI_DC_HIGH();
    SPI_send_DMA(buffer, size, repeat);
}


void fun_gdehxx_setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    write_cmd_8(0x44);
    write_data_8((x0 >> 3) &0xFF);
    write_data_8(x1);

    write_cmd_8(0x45);
    write_data_8(y0);
    write_data_8(0x00);
    write_data_8(y1);
    write_data_8(0x00);

    Delay_Ms(100);
}

void fun_gdehxx_setCursor(uint8_t x, uint16_t y) {
    write_cmd_8(0x4E);
    write_data_8((x >> 3) &0xFF);

    write_cmd_8(0x4F);
    write_data_8(y);
    write_data_8(0x00);

    Delay_Ms(100);
}


#define GDEHXX_WIDTH 128            // max 122 for 2.9" display
#define GDEHXX_HEIGHT 250           // max 250  for 2.9" display

void fun_gdehxx_update(uint8_t data) {
    write_cmd_8(0x22);          //Display Update Control
    write_data_8(data);         // 0xF7 for full update, 0xFF for partial update
    write_cmd_8(0x20);          //Activate Display Update Sequence
}

void fun_gdehxx_setMode(uint8_t mode) {
    write_cmd_8(0x11);

    switch (mode) {
        case 0:
            //# X decrement, Y decrement
            write_data_8(0b0100);
            fun_gdehxx_setWindow(GDEHXX_WIDTH-1, GDEHXX_HEIGHT-1, 0x00, 0x00);
            fun_gdehxx_setCursor(GDEHXX_WIDTH-1, GDEHXX_HEIGHT-1);
            break;
        
        case 1:
            // # X increment, Y decrement
            write_data_8(0b0101);
            fun_gdehxx_setWindow(0x00, GDEHXX_HEIGHT-1, GDEHXX_WIDTH-1, 0x00);
            fun_gdehxx_setCursor(0, GDEHXX_HEIGHT-1);
            break;

        case 2:
            //# X decrement, Y increment
            write_data_8(0b0110);
            fun_gdehxx_setWindow(GDEHXX_WIDTH-1, 0x00, 0x00, GDEHXX_HEIGHT-1);
            fun_gdehxx_setCursor(GDEHXX_WIDTH-1, 0x00);
            break;

        case 3:
            //# X increment, Y increment
            write_data_8(0b0111);
            fun_gdehxx_setWindow(0x00, 0x00, GDEHXX_WIDTH-1, GDEHXX_HEIGHT-1);
            fun_gdehxx_setCursor(0x00, 0x00);
            break;
    }

}


void fun_gdehxx_setup(uint8_t dc_pin, uint8_t rst_pin, uint8_t busy_pin) {
    GDEH_DC_PIN = dc_pin;

    funPinMode(rst_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
    funPinMode(dc_pin, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);

    funPinMode(PD0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PD0, 1);

    funPinMode(PA1, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA1, 1); 

    funDigitalWrite(dc_pin, 1);

    // Reset display
    funDigitalWrite(rst_pin, 0);
    Delay_Ms(2);
    funDigitalWrite(rst_pin, 1);
    Delay_Ms(2);

    //! Reset Display
    write_cmd_8(0x12);
    Delay_Ms(2);

    //! Init Code
    write_cmd_8(0x01);
    write_data_8(GDEHXX_HEIGHT-1);
    write_data_8(0x00);
    write_data_8(0b000);

    fun_gdehxx_setMode(1);
}


uint16_t double_bits_forByte(uint8_t byte) {
    uint16_t output = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (byte >> i) & 1;
        uint8_t doubled = bit ? 0b11 : 0b00;
        output = (output << 2) | doubled;
    }

    return output;
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
    const unsigned char* start = &font[(c-32)*width];

    for (uint8_t i = 0; i < width; i++) {
        buff[i] = *start++;
    }
}

void render_string_7x8(const char* str) {
    char char_buff[8] = {0};
    write_cmd_8(0x24);

    for (int i = 0; i<strlen(str); i++) {
        get_font_char(str[i], 7, &font7x8, char_buff);

        for (int j = 0; j < 7; j++) {
            char target = ~(char_buff[7-j]);
            write_data_8(target);            
        }
    }

    fun_gdehxx_update(0xF7);
}

void render_string_14x16(const char* str) {
    char char_buff[8] = {0};
    write_cmd_8(0x24);

    for (int i = 0; i < strlen(str); i++) {
        get_font_char(str[i], 7, &font7x8, char_buff);

        for (int j = 0; j < 7; j++) {
            char target = ~(char_buff[7-j]);
            const char halfTop = double_bits_forHalfByte(target, 1);
            write_data_8(halfTop);
            // write_data_8(halfTop);
        }
    }

    for (int i = 0; i < strlen(str); i++) {
        get_font_char(str[i], 7, &font7x8, char_buff);

        for (int j = 0; j < 7; j++) {
            char target = ~(char_buff[7-j]);
            const char halfBottom = double_bits_forHalfByte(target, 0);
            write_data_8(halfBottom);
            // write_data_8(halfBottom);
        }
    }

    fun_gdehxx_update(0xF7);
}

void fun_ghdehxx_fill(uint8_t byte) {
    write_cmd_8(0x24);

    // GDEHXX_WIDTH / 8 = GDEHXX_WIDTH >> 3
    for (int i = 0; i < (GDEHXX_WIDTH >> 3) * GDEHXX_HEIGHT; i++) {
        write_data_8(byte);
    }

    fun_gdehxx_update(0xF7);
}

void fund_ghdehxx_fillLen(uint8_t byte, uint16_t len) {
    write_cmd_8(0x24);

    for (int i = 0; i < len; i++) {
        write_data_8(byte);
    }

    fun_gdehxx_update(0xF7);

    while(1) {
        if (funDigitalRead(PC7) == 0) break;
    }
}

// #define TEST_FILLED

uint8_t refresh_count = 4;

void fun_gdehxx_task() {
    uint8_t read = funDigitalRead(PD0);

    if (read == 0) {
        fun_ghdehxx_fill(0xFF);
        refresh_count = 4;

    } else {
        if (refresh_count < 1) return;
        refresh_count--;

        #ifndef TEST_FILLED
            char my_str[] = "Hello world!";
            render_string_7x8(my_str);
        #else
            fund_ghdehxx_fillLen(0xAA, 10);
        #endif
    }
}