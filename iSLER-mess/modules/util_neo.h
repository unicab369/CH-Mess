#include "ch32fun.h"
#include <stdio.h>

#include "util_colors.h"

typedef struct {
    uint32_t color;
    uint32_t duration;      // in milliseconds
    int8_t index;
    int8_t count;           // -1 for infinite
    int8_t ref_count;       // last count value
    uint64_t ref_time;      // timestamp of the last change
} WS2812_blink_t;

typedef struct {
    RGB_t color;
    uint32_t frame_duration;    // Duration for each frame in ms
    int8_t frame_step;          // Step for moving LEDs
    uint8_t frame_value;         // Value for each frame
    
    int8_t cycle_count;
    
    uint8_t prev_index;         // Previous index used
    uint8_t ref_index;          // Last index used
    uint32_t ref_time;          // Last time the move was updated
} WS2812_move_t;


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