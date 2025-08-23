#include "ch32fun.h"
#include <stdio.h>

#define WS2812DMA_IMPLEMENTATION
#define WSGRB // For SK6805-EC15
#define NR_LEDS 6

#include "ws2812b_dma_spi_led_driver.h"
#include "util_neo.h"

RGB_t led_arr[NR_LEDS] = {0};

WS2812_blink_t blink_led = {
    .color = 0x0000A0, // Start with RED
    .duration = 50,   // 30 ms
    .index = 0,
    .count = 1,
    .ref_count = 0,
    .ref_time = 0
};

// uint32_t Neo_renderBlink(WS2812_blink_t* input, int ledIdx) {
//     uint32_t now = millis();

//     if (input->ref_count > 0 && ledIdx == input->index)  {
//         if (now - input->ref_time > input->duration) {
//             // Time to toggle the LED state
//             if (led_arr[ledIdx] == 0) {
//                 led_arr[ledIdx] = input->color; // Turn LED on
//             } else {
//                 led_arr[ledIdx] = 0; // Turn LED off
//                 input->ref_count--;
//             }

//             input->ref_time = now;
//         }
//     }

//     return led_arr[ledIdx];
// }



WS2812_move_t move_leds = {
    .color = COLOR_RED_HIGH,         
    .frame_duration = 100, 
    .frame_step = 1,            // Move one LED at a time
    .frame_value = 0,
};

void Neo_resetMoveLeds(uint32_t time) {
    move_leds.ref_index = 0;
    move_leds.prev_index = 0;
    move_leds.cycle_count = 5;
    move_leds.ref_time = time;

    move_leds.frame_value = 0;
    move_leds.color = COLOR_RED_HIGH;
}

uint32_t Neo_render_colorChase(WS2812_move_t* input, int ledIdx) {
    uint32_t now = millis();

    if (now - input->ref_time > input->frame_duration) {
        input->ref_time = now;
        led_arr[input->ref_index] = input->color;

        uint8_t next_increment = input->ref_index + input->frame_step;

        if (next_increment >= NR_LEDS) {
            RGB_t color = COLOR_EQUAL(input->color, COLOR_BLACK) ? COLOR_RED_HIGH : COLOR_BLACK;
            input->color = color;
        }

        input->ref_index = next_increment % NR_LEDS;
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_soloColorChase(WS2812_move_t* input, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        led_arr[input->prev_index] = COLOR_BLACK;       // Turn off previous LED
        led_arr[input->ref_index] = input->color;

        uint8_t next_increment = input->ref_index + input->frame_step;

        if (next_increment >= NR_LEDS) {
            if (input->cycle_count > 0) {
                input->cycle_count--;
            }
        }

        input->prev_index = input->ref_index;
        input->ref_index = next_increment % NR_LEDS;
        circular_buff_add(input->ref_index);
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_colorTrail(WS2812_move_t* input, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        // Fade all LEDs slightly
        for (int i = 0; i < NR_LEDS; i++) {
            uint8_t diff = input->ref_index - i;
            uint8_t decrement = diff*49;       // Triangular growth
            RGB_t value = COLOR_DECREMENT(input->color, decrement);
            led_arr[i] = value;
        }

        uint8_t next_increment = input->ref_index + input->frame_step;

        if (next_increment >= NR_LEDS) {
            if (input->cycle_count > 0) {
                input->cycle_count--;
            }
        }

        input->ref_index = next_increment % NR_LEDS;
    }
    
    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_soloColorTrail(WS2812_move_t* input, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        input->frame_value += 3;
        RGB_t color = COLOR_SET_BRIGHTNESS(input->color, input->frame_value);
        led_arr[input->ref_index] = color;

        circular_buff_add(input->frame_value);

        if (input->frame_value >= 100) {
            input->frame_value = 0;

            uint8_t next_increment = input->ref_index + input->frame_step;
            input->ref_index = next_increment % NR_LEDS;
        }
    }

    return led_arr[ledIdx].packed;
}


uint32_t WS2812BLEDCallback(int ledIdx){
    // return WS2812_renderBlink(&blink_led, ledIdx);

    // return Neo_render_colorChase(&move_leds, ledIdx);
    return Neo_render_soloColorChase(&move_leds, ledIdx);

    // return Neo_render_colorTrail(&move_leds, ledIdx);
    // return Neo_render_soloColorTrail(&move_leds, ledIdx);
}

void Neo_resetTask(uint32_t time) {
    blink_led.ref_count = blink_led.count;
    blink_led.ref_time = time;

    Neo_resetMoveLeds(time);
    
    ARRAY_PRINT(circular_buff, "%u");
    ARRAY_SET_VALUE(circular_buff, 0);

    // ARRAY_PRINT(sintable, "%u\n");

    // reset led_arr
    ARRAY_SET_VALUE(led_arr, 0);
}

void Neo_task() {
    if (WS2812BLEDInUse) return;

    WS2812BDMAStart(NR_LEDS);
}