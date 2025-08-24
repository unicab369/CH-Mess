#include "ch32fun.h"
#include <stdio.h>

#define WS2812DMA_IMPLEMENTATION
#define WSGRB // For SK6805-EC15
#define NR_LEDS 6

#include "ws2812b_dma_spi_led_driver.h"
#include "util_neo.h"

RGB_t led_arr[NR_LEDS] = {0};

WS2812_move_t move_leds = {
    .color = COLOR_RED_HIGH,         
    .frame_duration = 30, 
    .frame_step = 1,            // Move one LED at a time
    .frame_value = 0,

    .ref_index = 0,
    .prev_index = 0,
    .ref_time = 0,
};

RGB_t color_arr[] = {
    COLOR_RED_MED, COLOR_GREEN_MED, COLOR_BLUE_MED
};

animation_color_t color_ani = {
    .colors = color_arr,
    .num_colors = ARRAY_SIZE(color_arr),
    .ref_index = 0,
};

void Neo_resetMoveLeds(uint32_t time) {
    move_leds.ref_index = 0;
    move_leds.prev_index = 0;
    move_leds.cycle_count = 5;
    move_leds.ref_time = time;

    move_leds.frame_value = 0;
    move_leds.color = COLOR_RED_HIGH;
}

uint32_t Neo_render_colorChase(WS2812_move_t* input, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        for (int i=0; i < NR_LEDS; i++) {
            led_arr[i] = animation_colorAt(ani, 2, i+input->ref_index);
        }

        input->ref_index += input->frame_step;
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_soloColorChase(WS2812_move_t* input, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        led_arr[input->prev_index] = COLOR_BLACK;       // Turn off previous LED
        led_arr[input->ref_index] = animation_currentColor(ani);

        // set previous index
        input->prev_index = input->ref_index;

        // update next index
        uint8_t next_idx = input->ref_index + input->frame_step;
        input->ref_index = next_idx % NR_LEDS;
        
        // animation_step(ani);

        if (next_idx >= NR_LEDS) {
            animation_step(ani);
        }        
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_colorTrail(WS2812_move_t* input, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        // Fade all LEDs slightly
        for (int i = 0; i < NR_LEDS; i++) {
            uint8_t diff = input->ref_index - i;
            RGB_t color = animation_currentColor(ani);
            led_arr[i] = COLOR_DECREMENT(color, diff*49);       // Triangular diff growth
        }

        uint8_t next_increment = input->ref_index + input->frame_step;
        input->ref_index = next_increment % NR_LEDS;

        if (next_increment >= NR_LEDS) {
            animation_step(ani);
        }
    }
    
    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_soloColorTrail(WS2812_move_t* input, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&input->ref_time, input->frame_duration)) {
        input->frame_value += 3;
        RGB_t color = animation_currentColor(ani);
        led_arr[input->ref_index] = COLOR_SET_BRIGHTNESS(color, input->frame_value);

        if (input->frame_value >= 100) {
            input->frame_value = 0;

            uint8_t next_idx = input->ref_index + input->frame_step;
            input->ref_index = next_idx % NR_LEDS;

            if (next_idx >= NR_LEDS) {
                animation_step(ani);
            }
        }
    }

    return led_arr[ledIdx].packed;
}



uint32_t WS2812BLEDCallback(int ledIdx){
    // return Neo_render_colorChase(&move_leds, &color_ani, ledIdx);
    // return Neo_render_soloColorChase(&move_leds, &color_ani,ledIdx);

    // return Neo_render_colorTrail(&move_leds, &color_ani, ledIdx);
    return Neo_render_soloColorTrail(&move_leds, &color_ani, ledIdx);
}

void Neo_resetTask(uint32_t time) {
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