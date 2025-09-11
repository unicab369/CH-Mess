#include "ch32fun.h"
#include <stdio.h>

#include "../fun_epd.h"
#include "../../modules/systick_irq.h"


void getTimeDiff(uint32_t ref) {
    while(1) {
        if (funDigitalRead(PC7) == 0) break;
    }

    uint32_t timeDiff = millis() - ref;
    printf("Update took %lu ms\r\n", timeDiff);
    Delay_Ms(1);
}

int main() {
    SystemInit();
    systick_init();			//! required for millis()

    funGpioInitAll();
    Delay_Ms(100);

    SPI_init();
    fun_epd_setup(PC3, PC0, PC7);

    uint32_t now;
    uint32_t timeRef = millis();

    // funPinMode(PC7, GPIO_CFGLR_IN_PUPD);

    // // fill black
    // timeRef = millis();
    // fun_epd_fill(0x00, 0, EPD_HEIGHT-1);
    // getTimeDiff(timeRef);

    // fill white
    timeRef = millis();
    fun_epd_fill(0xFF, 0, EPD_HEIGHT-1);
    getTimeDiff(timeRef);
    
    // fun_gdehxx_setWindow(0x00, 100, GDEHXX_WIDTH-1, 0x00);
    // fun_epd_setCursor(0, 0);
    // timeRef = millis();
    // fun_epd_fill(0xAA);
    // getTimeDiff(timeRef);

    char my_str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~!#$&'()*+,-./0123456789:;<=>?@";
    timeRef = millis();
    render_string_6x8(my_str, 0, EPD_HEIGHT-1);
    getTimeDiff(timeRef);

    while(1) {
        now = millis();


        // if (now - timeRef > 200) {
        //     printf(".");
        //     timeRef = now;
        //     fun_gdehxx_task();
        // }
    }
}