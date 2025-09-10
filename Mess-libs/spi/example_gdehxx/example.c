#include "ch32fun.h"
#include <stdio.h>

#include "../fun_gdehxx.h"
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
    fun_gdehxx_setup(PC3, PC0, PC2);

    uint32_t now;
    uint32_t timeRef = millis();

    funPinMode(PC7, GPIO_CFGLR_IN_PUPD);

    // // fill black
    // timeRef = millis();
    // fun_ghdehxx_fill(0x00);
    // getTimeDiff(timeRef);

    // fill white
    fun_gdehxx_setCursor(0, GDEHXX_HEIGHT-1);
    timeRef = millis();
    fun_ghdehxx_fill(0xFF);
    getTimeDiff(timeRef);
    

    // fun_gdehxx_setWindow(0x00, 100, GDEHXX_WIDTH-1, 0x00);
    // fun_gdehxx_setCursor(0, 0);
    // timeRef = millis();
    // fun_ghdehxx_fill(0xAA);
    // getTimeDiff(timeRef);

    char my_str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~!#$&'()*+,-./0123456789:;<=>?@";
    timeRef = millis();
    render_string_12x8(my_str, 0, GDEHXX_HEIGHT-1);
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