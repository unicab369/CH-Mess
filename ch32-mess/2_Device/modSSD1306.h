#ifndef _MODSSD1306_H
#define _MODSSD1306_H

#include "ch32fun.h"
#include "modI2C.h"

#define CHUNK_SIZE 32  		// Define your desired chunk size
#define SSD1306_PAGES       (SSD1306_H / 8) // 8 pages for 64 rows
// #define SSD1306_PAGES       1 // 8 pages for 64 rows

#define SSD1306_W_LIMIT	SSD1306_W - 1
#define SSD1306_H_LIMIT	SSD1306_H - 1

#define M_PI 3.14

uint8_t frame_buffer[SSD1306_PAGES][SSD1306_W] = { 0 };

typedef struct {
	uint8_t x;
	uint8_t y;
} M_Point;

typedef struct {
	uint8_t w;
	uint8_t h;
} Area;

typedef struct {
	uint8_t l0;
	uint8_t l1;
} Limit;

typedef struct {
	uint8_t page;
	uint8_t bitmask;
} M_Page_Mask;

M_Page_Mask page_masks[SSD1306_H];

void precompute_page_masks() {
	for (uint8_t y = 0; y < SSD1306_H; y++) {
		page_masks[y].page       = y >> 3;             // (y / 8)
		page_masks[y].bitmask    = 1 << (y & 0x07);    // (y % 8)
	}
}


void ssd1306_setwindow(uint8_t start_page, uint8_t end_page, uint8_t start_column, uint8_t end_column) {
	ssd1306_cmd(SSD1306_COLUMNADDR);
	ssd1306_cmd(start_column);   				// Column start address (0 = reset)
	ssd1306_cmd(end_column); 	// Column end address (127 = reset)
	
	ssd1306_cmd(SSD1306_PAGEADDR);
	ssd1306_cmd(start_page); 	// Page start address (0 = reset)
	ssd1306_cmd(end_page); 		// Page end address
}

void ssd1306_setWindow_pages(uint8_t start_page, uint8_t end_page) {
	ssd1306_setwindow(start_page, end_page, 0, SSD1306_W_LIMIT); // Set the window to the current page
}

void ssd1306_print_str_at(
  const char *str, uint8_t page, uint8_t column
) {
	ssd1306_setWindow_pages(page, page); // Set the window to the current page

	for (int i=0; i<25; i++) {
		if (*str) {
			uint8_t char_index = *str - 32; // Adjust for ASCII offset
			ssd1306_data((uint8_t *)FONT_7x5[char_index], 5); // Send font data
			str++;
		} else {
			ssd1306_data((uint8_t *)FONT_7x5[0], 5); // Send space character if no more characters
		}
	}
}

//! render area
void ssd1306_renderArea(
	uint8_t start_page, uint8_t end_page,
	uint8_t col_start, uint8_t col_end
) {
	ssd1306_setwindow(start_page, end_page, col_start, col_end-1);

    for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
        // Send page data in chunks
        for (uint16_t chunk = 0; chunk < col_end; chunk += CHUNK_SIZE) {
			uint16_t chunk_end = chunk + CHUNK_SIZE;
			if (chunk_end > col_end) chunk_end = col_end;
            ssd1306_data(&frame_buffer[page][chunk], chunk_end - chunk);
        }
    }
}

//! render the entire screen
void ssd1306_renderFrame() {
	ssd1306_renderArea(0, 7, 0, SSD1306_W);
}

//! compute pixel
void prefill_pixel(uint8_t x, uint8_t y) {
    if (x >= SSD1306_W || y >= SSD1306_H) return; // Skip if out of bounds
    M_Page_Mask mask = page_masks[y];
    frame_buffer[mask.page][x] |= mask.bitmask;
}

void ssd1306_setup() {
	precompute_page_masks();
	ssd1306_init();

	// //# SSD1306
	// if(!ssd1306_i2c_init()) {
	// 	ssd1306_init();
	// 	printf("SSD1306 init.\n\r");
	// }
	
	//# Clear the frame buffer
	memset(frame_buffer, 0, sizeof(frame_buffer));
	ssd1306_renderFrame();
}

#endif