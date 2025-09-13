#include "ch32fun.h"
#include <stdio.h>

#define WS2812DMA_IMPLEMENTATION
#define WSGRB // For SK6805-EC15
#define NR_LEDS 12
#include "ws2812b_dma_spi_led_driver.h"
#include "util_colors.h"

typedef struct {
    RGB_t color;
    uint32_t frame_duration;    // Duration for each frame in ms
    int8_t frame_step;          // Step for moving LEDs
    uint8_t frame_value;        // Value for each frame
    uint8_t frame_LEN;          // number of LEDs
    uint8_t is_enabled;
    int8_t cycle_count;
    
    uint8_t prev_index;         // Previous index used
    uint8_t ref_index;          // Last index used
    uint32_t ref_time;          // Last time the move was updated
} WS2812_frame_t;

typedef struct {
    RGB_t* colors;          // Array of colors to cycle through
    uint8_t num_colors;     // Number of colors in the array
    uint8_t ref_index;      // Current color index
} animation_color_t;


void animation_step(animation_color_t* ani) {
    ani->ref_index = (ani->ref_index + 1) % ani->num_colors;
}

RGB_t animation_currentColor(animation_color_t* ani) {
    return ani->colors[ani->ref_index];
}

RGB_t animation_colorAt(animation_color_t* ani, uint8_t steps, uint8_t index) {
    return ani->colors[(index/steps) % ani->num_colors];
}


typedef enum {
    NEO_COLOR_CHASE = 0x01,
    NEO_SOLO_COLOR_CHASE = 0x02,
    NEO_COLOR_FADE = 0x03,
    NEO_SOLO_COLOR_FADE = 0x04,
    NEO_COLOR_FLASHING = 0x05,
} Neo_Event_e;

Neo_Event_e Neo_Event_list[] = {
    NEO_COLOR_CHASE,
    NEO_SOLO_COLOR_CHASE,
    NEO_COLOR_FADE,
    NEO_SOLO_COLOR_FADE,
    NEO_COLOR_FLASHING
};

RGB_t led_arr[NR_LEDS] = {0};

WS2812_frame_t leds_frame = {     
    .frame_duration = 70, 
    .frame_step = 1,            // Move one LED at a time
    .frame_value = 0,
    .is_enabled = 0,

    .ref_index = 0,
    .prev_index = 0,
    .ref_time = 0,
};

RGB_t color_arr[] = {
    COLOR_RED_LOW, COLOR_GREEN_LOW, COLOR_BLUE_LOW
};

animation_color_t color_ani = {
    .colors = color_arr,
    .num_colors = sizeof(color_arr)/sizeof(RGB_t),
    .ref_index = 0,
};

int8_t systick_handleTimeout(uint32_t *ref_time, uint32_t duration) {
	uint32_t now = millis();
	if (now - *ref_time > duration) {
		*ref_time = now;
		return 1;
	}
	return 0;
}

uint32_t Neo_render_colorChase(WS2812_frame_t* fr, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&fr->ref_time, fr->frame_duration)) {
        for (int i=0; i < NR_LEDS; i++) {
            led_arr[i] = animation_colorAt(ani, 5, i+fr->ref_index);
        }

        fr->ref_index += fr->frame_step;
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_soloColorChase(WS2812_frame_t* fr, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&fr->ref_time, fr->frame_duration)) {
        led_arr[fr->prev_index] = COLOR_BLACK;       // Turn off previous LED
        led_arr[fr->ref_index] = animation_currentColor(ani);

        // set previous index
        fr->prev_index = fr->ref_index;

        // update next index
        uint8_t next_idx = fr->ref_index + fr->frame_step;
        fr->ref_index = next_idx % NR_LEDS;
        
        // animation_step(ani);
        if (next_idx >= NR_LEDS) animation_step(ani);
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_colorFade(WS2812_frame_t* fr, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&fr->ref_time, fr->frame_duration)) {
        // Fade all LEDs slightly
        for (int i = 0; i < NR_LEDS; i++) {
            uint8_t diff = fr->ref_index - i;
            RGB_t color = animation_currentColor(ani);
            led_arr[i] = COLOR_DECREMENT(color, diff*49);       // Triangular diff growth
        }

        uint8_t next_increment = fr->ref_index + fr->frame_step;
        fr->ref_index = next_increment % NR_LEDS;

        if (next_increment >= NR_LEDS) {
            animation_step(ani);
        }
    }
    
    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_soloColorFade(WS2812_frame_t* fr, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&fr->ref_time, fr->frame_duration)) {
        fr->frame_value += 3;
        RGB_t color = animation_currentColor(ani);
        led_arr[fr->ref_index] = COLOR_SET_BRIGHTNESS(color, fr->frame_value);

        if (fr->frame_value >= 100) {
            fr->frame_value = 0;

            uint8_t next_idx = fr->ref_index + fr->frame_step;
            fr->ref_index = next_idx % NR_LEDS;

            if (next_idx >= NR_LEDS) {
                animation_step(ani);
            }
        }
    }

    return led_arr[ledIdx].packed;
}

uint32_t Neo_render_colorFlashing(WS2812_frame_t* fr, animation_color_t* ani, int ledIdx) {
    if (systick_handleTimeout(&fr->ref_time, fr->frame_duration)) {
        fr->frame_value += 1;
        RGB_t color = animation_currentColor(ani);

        for (int i=0; i < NR_LEDS; i++) {
            led_arr[i] = COLOR_SET_BRIGHTNESS(color, fr->frame_value);
        }

        if (fr->frame_value >= 100) {
            fr->frame_value = 0;

            animation_step(ani);
        }
    }

    return led_arr[ledIdx].packed;
}

uint8_t Neo_LedCmd = 0x61;

void Neo_loadCommand(uint8_t cmd) {
    printf("Neo_loadCommand: %02X\n", cmd);

    Neo_LedCmd = cmd;
    leds_frame.is_enabled = 1;
    leds_frame.ref_index = 0;
    leds_frame.ref_time = millis();

    color_ani.ref_index = 0;
    memset(led_arr, 0, sizeof(led_arr));
}

uint32_t WS2812BLEDCallback(int ledIdx){
    leds_frame.frame_duration = 70;

    switch (Neo_LedCmd) {
        case NEO_COLOR_CHASE:
            return Neo_render_colorChase(&leds_frame, &color_ani, ledIdx);
            break;
        case NEO_SOLO_COLOR_CHASE:
            return Neo_render_soloColorChase(&leds_frame, &color_ani, ledIdx);
            break;
        case NEO_COLOR_FADE:
            return Neo_render_colorFade(&leds_frame, &color_ani, ledIdx);
            break;
        case NEO_SOLO_COLOR_FADE:
            leds_frame.frame_duration = 10;
            return Neo_render_soloColorFade(&leds_frame, &color_ani, ledIdx);
            break;
        case NEO_COLOR_FLASHING:
            leds_frame.frame_duration = 10;
            return Neo_render_colorFlashing(&leds_frame, &color_ani, ledIdx);
            break;
        default:
            return Neo_render_colorFlashing(&leds_frame, &color_ani, ledIdx);
    }
}

uint32_t neo_timeRef = 0;

void Neo_task(uint32_t time) {
    if (time - neo_timeRef < 10) return;
    neo_timeRef = time;
    
    if (WS2812BLEDInUse || leds_frame.is_enabled == 0) return;

    WS2812BDMAStart(NR_LEDS);
}