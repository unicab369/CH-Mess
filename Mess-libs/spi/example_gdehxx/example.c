#include "ch32fun.h"
#include <stdio.h>

#include "../fun_gdehxx.h"
#include "../../modules/systick_irq.h"

int main() {
    SystemInit();
    systick_init();			//! required for millis()

    funGpioInitAll();
    Delay_Ms(100);

    SPI_init();
    fun_gdehxx_setup(PC3, PC0, PC2);

    uint32_t now = millis();
    uint32_t timeRef = now;

    while(1) {
        now = millis();

        if (now - timeRef > 200) {
            printf(".");
            
            timeRef = now;
            fun_gdehxx_task();
        }
    }
}