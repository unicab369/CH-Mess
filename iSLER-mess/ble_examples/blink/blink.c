#include "ch32fun.h"
#include <stdio.h>

// Connect active-high LEDs to any of these pins to see the blink pattern.
#define PIN_1     PA4
#define PIN_K     PA5
#define PIN_BOB   PA8
#define PIN_KEVIN PA9

// Watch these in the debugger to see the latest and longest loop duration.
volatile uint32_t blink_loop_us;
volatile uint32_t blink_loop_max_us;

int main(void) {
    SystemInit();
    funGpioInitAll();

    funPinMode(PIN_1, GPIO_CFGLR_OUT_10Mhz_PP);
    funPinMode(PIN_K, GPIO_CFGLR_OUT_10Mhz_PP);
    funPinMode(PIN_BOB, GPIO_CFGLR_OUT_10Mhz_PP);
    funPinMode(PIN_KEVIN, GPIO_CFGLR_OUT_10Mhz_PP);

    uint32_t next_loop_report = (uint32_t)(funSysTick64() / DELAY_MS_TIME) + 2000;

    for (;;) {
        uint64_t loop_start_ticks = funSysTick64();

        // funDigitalWrite(PIN_1, FUN_HIGH);
        // funDigitalWrite(PIN_K, FUN_HIGH);
        // funDigitalWrite(PIN_BOB, FUN_HIGH);
        // funDigitalWrite(PIN_KEVIN, FUN_HIGH);
        // Delay_Ms(250);

        // funDigitalWrite(PIN_1, FUN_LOW);
        // funDigitalWrite(PIN_K, FUN_LOW);
        // funDigitalWrite(PIN_BOB, FUN_LOW);
        // funDigitalWrite(PIN_KEVIN, FUN_LOW);
        // Delay_Ms(250);

        uint32_t loop_us = (uint32_t)((funSysTick64() - loop_start_ticks) /
                                      DELAY_US_TIME);
        blink_loop_us = loop_us;
        if (loop_us > blink_loop_max_us)
            blink_loop_max_us = loop_us;

        uint32_t report_now = (uint32_t)(funSysTick64() / DELAY_MS_TIME);
        if ((int32_t)(report_now - next_loop_report) >= 0) {
            next_loop_report = report_now + 2000;
            printf("Loop: %lu us, max: %lu us\n",
                   (unsigned long)blink_loop_us,
                   (unsigned long)blink_loop_max_us);
        }
    }
}
