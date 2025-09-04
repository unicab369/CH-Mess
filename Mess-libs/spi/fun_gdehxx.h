#include "ch32fun.h"
#include <stdint.h>
#include "lib_tft.h"
#include "lib_spi.h"

#include "font7x8.h"

static uint8_t GDEH_DC_PIN;


void INTF_SPI_DC_LOW()  { funDigitalWrite(GDEH_DC_PIN, 0); }
void INTF_SPI_DC_HIGH() { funDigitalWrite(GDEH_DC_PIN, 1); }

void INTF_TFT_SEND_BUFF(const uint8_t* buffer, uint16_t size, uint16_t repeat) {
    INTF_SPI_DC_HIGH();
    SPI_send_DMA(buffer, size, repeat);
}


void fun_gdehxx_setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    write_cmd_8( 0x44 );
    write_data_8( (x0 >> 3) &0xFF );
    write_data_8( x1 );

    write_cmd_8( 0x45 );
    write_data_8( y0 );
    write_data_8( 0x00 );
    write_data_8( y1 );
    write_data_8( 0x00 );

    Delay_Ms(100);
}

void fun_gdehxx_setCursor(uint8_t x, uint16_t y) {
    write_cmd_8( 0x4E );
    write_data_8( (x >> 3) &0xFF );

    write_cmd_8( 0x4F );
    write_data_8( y );
    write_data_8( 0x00 );

    Delay_Ms(100);
}


#define GDEHXX_WIDTH 120            // max 122 for 2.9" display
#define GDEHXX_HEIGHT 200           // max 250  for 2.9" display

void fun_gdehxx_update(uint8_t data) {
    write_cmd_8(0x22);          //Display Update Control
    write_data_8(data);         // 0xF7 for full update, 0xFF for partial update
    write_cmd_8(0x20);          //Activate Display Update Sequence
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
    Delay_Ms(15);
    funDigitalWrite(rst_pin, 1);
    Delay_Ms(15);

    //! Reset Display
    write_cmd_8(0x12);
    Delay_Ms(100);

    //! Init Code
    write_cmd_8(0x01);
    write_data_8(GDEHXX_HEIGHT-1);
    write_data_8(0x00);
    write_data_8(0b001);


    /////////////////////////////
    //# X decrement, Y decrement
    write_cmd_8(0x11);
    write_data_8(0b0100);

    //! setWindow
    fun_gdehxx_setWindow(GDEHXX_WIDTH-1, GDEHXX_HEIGHT-1, 0x00, 0x00);
    fun_gdehxx_setCursor(0x00, 0x00);


    // ///////////////////////////////
    // // # X increment, Y decrement
    // write_cmd_8(0x11);
    // write_data_8(0b0101);

    // //! setWindow
    // fun_gdehxx_setWindow(0x00, GDEHXX_HEIGHT-1, GDEHXX_WIDTH-1, 0x00);
    // fun_gdehxx_setCursor(0, GDEHXX_WIDTH-1);


    // ////////////////////////////////
    // //# X decrement, Y increment
    // write_cmd_8(0x11);
    // write_data_8(0b0110);

    // //! setWindow
    // fun_gdehxx_setWindow(GDEHXX_WIDTH-1, 0x00, 0x00, GDEHXX_HEIGHT-1);
    // fun_gdehxx_setCursor(GDEHXX_WIDTH-1, 0x00);


    // ///////////////////////////
    // //# X increment, Y increment
    // write_cmd_8(0x11);
    // write_data_8(0b0111);

    // //! setWindow
    // fun_gdehxx_setWindow(0x00, 0x00, GDEHXX_WIDTH-1, GDEHXX_HEIGHT-1);
    // fun_gdehxx_setCursor(0x00, 0x00);

    Delay_Ms(100);
}


char _frame_buffer2[7] = {0};

void tft_print_char2(
    char c, uint8_t width, const char *font
) {
    const unsigned char* start = &font[(c-32)*width];

    uint16_t sz = 0;
    for (uint8_t i = 0; i < width; i++) {
        _frame_buffer2[i] = *start++;
    }
}


void fun_gdehxx_task() {
    if (funDigitalRead(PA1) == 0) {
        return;    
    }

    write_cmd_8(0x24);

    uint8_t read = funDigitalRead(PD0);
    uint8_t color;

    if (read == 0) {
        for (int i = 0; i < 5000; i++) {
            write_data_8(0xFF);
        }
        fun_gdehxx_setCursor(0, 0);

    } else {
        char my_str[] = "Hello, world!";

        for (int i = 0; i < strlen(my_str); i++) {
            tft_print_char2(my_str[i], 7, &font7x8);

            for (int j = 0; j < 7; j++) {
                write_data_8(_frame_buffer2[7-j]);            
            }
        }

        // for (int i = 0; i < 5; i++) {
        //     write_data_8(0xAA);
        // }
    }

    fun_gdehxx_update(0xF7);
}