// stolen and adjusted from: GitHub: https://github.com/limingjie/

#include "ch32fun.h"
#include "font5x7.h"
#include "modTFT_line.h"

#define ST7735_W    160
#define ST7735_H    80


static uint8_t  _frame_buffer[ST7735_W << 1] = {0};


void tft_print_char(
    char c, uint8_t height, uint8_t width,
    uint16_t color, uint16_t bg_color
) {
    const unsigned char* start = &font[c + (c << 2)];

    uint16_t sz = 0;
    for (uint8_t i = 0; i < height; i++) {
        for (uint8_t j = 0; j < width; j++) {
            if ((*(start + j)) & (0x01 << i)) {
                _frame_buffer[sz++] = color >> 8;
                _frame_buffer[sz++] = color;
            }
            else {
                _frame_buffer[sz++] = bg_color >> 8;
                _frame_buffer[sz++] = bg_color;
            }
        }
    }

    // START_WRITE();
    tft_set_window(_cursor_x, _cursor_y, _cursor_x + width - 1, _cursor_y + height - 1);
    tft_send_DMA(_frame_buffer, sz, 1);
    // END_WRITE();
}

void tft_print(const char* str) {
    uint8_t font_width = 5; // Assuming a fixed width for the font

    while (*str) {
        tft_print_char(*str++, 7, font_width, 0xFFFF, 0x0000); // 7x5 font size
        _cursor_x += font_width + 1;
    }
}

void tft_fill_rect(
    uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color
) {
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    uint16_t sz = 0;
    for (uint16_t x = 0; x < width; x++) {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    tft_set_window(x, y, x + width - 1, y + height - 1);
    tft_send_DMA(_buffer, sz, height);
    END_WRITE();
}

void tft_print_number(int32_t num, uint16_t width) {
    static char str[12];
    uint8_t     position  = 11;
    uint8_t     negative  = 0;
    uint16_t    num_width = 0;

    // Handle negative number
    if (num < 0) {
        negative = 1;
        num      = -num;
    }

    str[position] = '\0';  // End of the string.
    while (num) {
        str[--position] = num % 10 + '0';
        num /= 10;
    }

    if (position == 11) str[--position] = '0';

    if (negative) str[--position] = '-';
    
    // Calculate alignment
    num_width = (11 - position) * (FONT_WIDTH + 1) - 1;
    if (width > num_width) {
        _cursor_x += width - num_width;
    }

    tft_print(&str[position]);
}


void tft_draw_bitmap(
    uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, const uint8_t* bitmap
) {
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    START_WRITE();
    tft_set_window(x, y, x + width - 1, y + height - 1);
    tft_send_DMA(bitmap, width * height << 1, 1);
    END_WRITE();
}



void modST7735_setup(uint8_t rst_pin, uint8_t dc_pin) {
    tft_init(rst_pin, dc_pin);
    tft_fill_rect(0, 0, 160, 128, PURPLE);
}



void tft_test() {
    tft_set_cursor(0, 0);
    tft_print("Hello World!");
    tft_print_number(123456789, 0);


    tft_line_tests();


    // draw rectangles
    static uint8_t rect_idx = 0;
    tft_draw_rect(rect_idx, rect_idx, 160 - (rect_idx << 1), 80 - (rect_idx << 1), colors[rand8() % 19]);
    rect_idx += 1;
    if (rect_idx >= 40) rect_idx = 0;

    // draw random rectangles
    tft_draw_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);

    // draw filled rectangles
    tft_fill_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);


    // static uint8_t x = 0, y = 0, step_x = 1, step_y = 1;

    // uint16_t bg = colors[rand8() % 19];
    // tft_fill_rect(x, y, 88, 17, bg);
    // tft_set_color(colors[rand8() % 19]);
    // tft_set_background_color(bg);
    // tft_set_cursor(x + 5, y + 5);
    // tft_print("Hello, World!");
    // // Delay_Ms(25);

    // x += step_x;
    // if (x >= 72) step_x = -step_x;
    // y += step_y;
    // if (y >= 63) step_y = -step_y;
}


static uint32_t frame = 0;

int st7735_test(void) {
    // tft_set_color(RED);
    tft_set_background_color(BLACK);

    // popup("Draw Point", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame = 30000;
    // while (frame-- > 0)
    // {
    //     tft_draw_pixel(rand8() % 160, rand8() % 80, colors[rand8() % 19]);
    // }

    // popup("Scan Line", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame = 50;
    // while (frame-- > 0)
    // {
    //     for (uint8_t i = 0; i < 160; i++)
    //     {
    //         tft_draw_line(i, 0, i, 80, colors[rand8() % 19]);
    //     }
    // }
    // frame = 50;
    // while (frame-- > 0)
    // {
    //     for (uint8_t i = 0; i < 80; i++)
    //     {
    //         tft_draw_line(0, i, 180, i, colors[rand8() % 19]);
    //     }
    // }

    // popup("Draw Line", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame = 2000;
    // while (frame-- > 0)
    // {
    //     tft_draw_line(rand8() % 160, rand8() % 80, rand8() % 160, rand8() % 80, colors[rand8() % 19]);
    // }

    // popup("Scan Rect", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame = 100;
    // while (frame-- > 0)
    // {
    //     for (uint8_t i = 0; i < 40; i++)
    //     {
    //         tft_draw_rect(i, i, 160 - (i << 1), 80 - (i << 1), colors[rand8() % 19]);
    //     }
    // }

    // popup("Draw Rect", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame = 5000;
    // while (frame-- > 0)
    // {
    //     tft_draw_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);
    // }

    // popup("Fill Rect", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame = 5000;
    // while (frame-- > 0)
    // {
    //     tft_fill_rect(rand8() % 140, rand8() % 60, 20, 20, colors[rand8() % 19]);
    // }

    // popup("Move Text", 1000);
    // tft_fill_rect(0, 0, 160, 80, BLACK);

    // frame     = 500;
    // uint8_t x = 0, y = 0, step_x = 1, step_y = 1;
    // while (frame-- > 0)
    // {
    //     uint16_t bg = colors[rand8() % 19];
    //     tft_fill_rect(x, y, 88, 17, bg);
    //     tft_set_color(colors[rand8() % 19]);
    //     tft_set_background_color(bg);
    //     // tft_set_cursor(x + 5, y + 5);
    //     // tft_print("Hello, World!");
    //     Delay_Ms(25);

    //     x += step_x;
    //     if (x >= 72)
    //     {
    //         step_x = -step_x;
    //     }
    //     y += step_y;
    //     if (y >= 63)
    //     {
    //         step_y = -step_y;
    //     }
    // }
}
