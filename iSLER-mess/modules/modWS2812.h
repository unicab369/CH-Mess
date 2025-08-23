#include "ch32fun.h"
#include <stdio.h>

#define WS2812DMA_IMPLEMENTATION
#define WSGRB // For SK6805-EC15
#define NR_LEDS 10

#include "ws2812b_dma_spi_led_driver.h"
#include "color_utilities.h"

#define CIRCULAR_BUFF_SIZE 12
uint8_t circular_buff_idx = 0;
uint32_t circular_buff[CIRCULAR_BUFF_SIZE] = {0};

void circular_buff_add(uint32_t value) {
    circular_buff[circular_buff_idx] = value;
    circular_buff_idx += 1;

    if (circular_buff_idx >= CIRCULAR_BUFF_SIZE) {
        circular_buff_idx = 0;
    }
}   

uint32_t led_arr[NR_LEDS] = {0};

typedef struct {
    uint32_t color;
    uint32_t duration;      // in milliseconds
    int8_t index;
    int8_t count;           // -1 for infinite
    int8_t ref_count;       // last count value
    uint64_t ref_time;      // timestamp of the last change
} WS2812_blink_t;

WS2812_blink_t blink_led = {
    .color = 0x0000A0, // Start with RED
    .duration = 50,   // 30 ms
    .index = 0,
    .count = 1,
    .ref_count = 0,
    .ref_time = 0
};

uint32_t WS2812_renderBlink(WS2812_blink_t* input, int ledIdx) {
    uint32_t now = millis();

    if (input->ref_count > 0 && ledIdx == input->index)  {
        if (now - input->ref_time > input->duration) {
            // Time to toggle the LED state
            if (led_arr[ledIdx] == 0) {
                led_arr[ledIdx] = input->color; // Turn LED on
            } else {
                led_arr[ledIdx] = 0; // Turn LED off
                input->ref_count--;
            }

            input->ref_time = now;
        }
    }

    return led_arr[ledIdx];
}

typedef struct {
    int32_t color;
    uint32_t frame_duration;    // Duration for each frame in ms
    int8_t frame_step;          // Step for moving LEDs
    int8_t cycle_count;         // -1 for infinite
    
    uint8_t ref_index;          // Last index used
    uint32_t ref_time;          // Last time the move was updated
} WS2812_move_t;

WS2812_move_t move_leds = {
    .color = 0x0000A0,          // Start with RED
    .frame_duration = 100, 
    .frame_step = 1,            // Move one LED at a time

    .ref_index = 0,
    .ref_time = 0
};


void WS2812_resetMoveLeds(uint32_t time) {
    move_leds.ref_index = 0;
    move_leds.cycle_count = 5;
    move_leds.ref_time = time;
}

uint32_t WS2812_renderMove(WS2812_move_t* input, int ledIdx) {
    uint32_t now = millis();

    if (now - input->ref_time > input->frame_duration) {
        input->ref_time = now;
        led_arr[input->ref_index] = input->color;

        uint8_t next_increment = input->ref_index + input->frame_step;

        if (next_increment >= NR_LEDS) {
            if (input->cycle_count > 0) {
                input->cycle_count--;
            }
        }

        input->ref_index = next_increment % NR_LEDS;
        circular_buff_add(input->ref_index);
    }

    return led_arr[ledIdx];
}

uint32_t WS2812BLEDCallback(int ledIdx){
    // return WS2812_renderBlink(&blink_led, ledIdx);
    return WS2812_renderMove(&move_leds, ledIdx);
}

void WS2812_resetTask(uint32_t time) {
    printf("****Resetting WS2812 task\n");

    blink_led.ref_count = blink_led.count;
    blink_led.ref_time = time;

    WS2812_resetMoveLeds(time);
    
    // print circular buffer
    printf("\nCircular buffer: ");
    for (int i = 0; i < CIRCULAR_BUFF_SIZE; i++) {
        printf("%02X ", circular_buff[i]);
    }
    printf("\n");
    memset(circular_buff, 0, sizeof(circular_buff));

    // reset led_arr
    memset(led_arr, 0, sizeof(led_arr));
}

void WS2812_task() {
    if (WS2812BLEDInUse) return;

    WS2812BDMAStart(NR_LEDS);
}