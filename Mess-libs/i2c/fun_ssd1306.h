// MIT License

// Copyright (c) 2026 unicab369
// The original code was code was from E. Brombaugh ssd1306 implementation in ch32fun
// but I removed and add different methods which speed up the rendering at the cost
// of a little more Flash
/*
 * Single-File-Header for using SPI OLED
 * 05-05-2023 E. Brombaugh
 */

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#ifndef _MODSSD1306_H
#define _MODSSD1306_H

#include "ch32fun.h"
#include <stdlib.h>

#define SSD1306_128X64

#include <stdint.h>
#include <string.h>
#include "word_font.h"

// comfortable packet size for this OLED
#define SSD1306_PSZ 32

#if defined (SSD1306_CUSTOM)
// Let the caller configure the OLED.
#else
// characteristics of each type
#if !defined(SSD1306_64X32) && \
	!defined(SSD1306_128X32) && \
	!defined(SSD1306_128X64) && \
	!defined(SH1107_128x128) && \
	!(defined(SSD1306_W) && defined(SSD1306_H) && defined(SSD1306_OFFSET))
	#error "Please define the SSD1306_WXH resolution used in your application"
#endif

#ifdef SSD1306_64X32
#define SSD1306_W 64
#define SSD1306_H 32
#define SSD1306_FULLUSE
#define SSD1306_OFFSET 32
#endif

#ifdef SSD1306_128X32
#define SSD1306_W 128
#define SSD1306_H 32
#define SSD1306_OFFSET 0
#endif

#ifdef SSD1306_128X64
#define SSD1306_W 128
#define SSD1306_H 64
#define SSD1306_FULLUSE
#define SSD1306_OFFSET 0
#endif

#ifdef SH1107_128x128
#define SH1107
#define SSD1306_FULLUSE
#define SSD1306_W 128
#define SSD1306_H 128
#define SSD1306_FULLUSE
#define SSD1306_OFFSET 0
#endif

#endif


uint8_t SSD1306_CMD(uint8_t cmd);
uint8_t SSD1306_DATA(uint8_t *data, int sz);

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2
#define SSD1306_TERMINATE_CMDS 0xFF

/* choose VCC mode */
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2
//#define vccstate SSD1306_EXTERNALVCC
#define vccstate SSD1306_SWITCHCAPVCC

#if !defined(SSD1306_CUSTOM_INIT_ARRAY) || !SSD1306_CUSTOM_INIT_ARRAY
// OLED initialization commands for 128x32
const uint8_t ssd1306_init_array[] = {
#ifdef SH1107
	SSD1306_DISPLAYOFF,               // Turn OLED off
	0x00,                             // Low column
	0x10,                             // High column
	0xb0,                             // Page address
	0xdc, 0x00,                       // Set Display Start Line  (Where in memory it reads from)
	SSD1306_SETCONTRAST, 0x6f,        // Set constrast
	SSD1306_COLUMNADDR,               // Set memory addressing mode
	SSD1306_DISPLAYALLON_RESUME,      // normal (as opposed to invert colors, always on or off.)
	SSD1306_SETMULTIPLEX, (SSD1306_H-1), // Iterate over all 128 rows (Multiplex Ratio)
	SSD1306_SETDISPLAYOFFSET, 0x00,   // Set display offset // Where this appears on-screen  (Some displays will be different)
	SSD1306_SETDISPLAYCLOCKDIV, 0xf0, // Set precharge properties.  THIS IS A LIE  This has todo with timing.  <<< This makes it go brrrrrrrrr
	SSD1306_SETPRECHARGE, 0x1d,       // Set pre-charge period  (This controls brightness)
	SSD1306_SETVCOMDETECT, 0x35,      // Set vcomh
	SSD1306_SETSTARTLINE | 0x0,            // 0x40 | line
	0xad, 0x80,                       // Set Charge pump
	SSD1306_SEGREMAP, 0x01,           // Default mapping
	SSD1306_SETPRECHARGE, 0x06,       // ???? No idea what this does, but this looks best.
	SSD1306_SETCONTRAST, 0xfe,        // Set constrast
	SSD1306_SETVCOMDETECT, 0xfe,      // Set vcomh
	SSD1306_SETMULTIPLEX, (SSD1306_H-1), // 128-wide.
	SSD1306_DISPLAYON, // Display on.
#else
	SSD1306_DISPLAYOFF,                    // 0xAE
	SSD1306_SETDISPLAYCLOCKDIV,            // 0xD5
	0x80,                                  // the suggested ratio 0x80
	SSD1306_SETMULTIPLEX,                  // 0xA8
#ifdef SSD1306_64X32
	0x1F,                                  // for 64-wide displays
#else
	0x3F,                                  // for 128-wide displays
#endif
	SSD1306_SETDISPLAYOFFSET,              // 0xD3
	0x00,                                  // no offset
	SSD1306_SETSTARTLINE | 0x0,            // 0x40 | line
	SSD1306_CHARGEPUMP,                    // 0x8D
	0x14,                                  // enable?
	SSD1306_MEMORYMODE,                    // 0x20
	0x00,                                  // 0x0 act like ks0108
	SSD1306_SEGREMAP | 0x1,                // 0xA0 | bit
	SSD1306_COMSCANDEC,
	SSD1306_SETCOMPINS,                    // 0xDA
	0x12,                                  //
	SSD1306_SETCONTRAST,                   // 0x81
	0x8F,
	SSD1306_SETPRECHARGE,                  // 0xd9
	0xF1,
	SSD1306_SETVCOMDETECT,                 // 0xDB
	0x40,
	SSD1306_DISPLAYALLON_RESUME,           // 0xA4
#ifndef SSD1327
	SSD1306_NORMALDISPLAY,                 // 0xA6
#endif
	SSD1306_DISPLAYON,	                   // 0xAF --turn on oled panel
#endif
	SSD1306_TERMINATE_CMDS                 // 0xFF --fake command to mark end
};
#endif

// the display buffer
uint8_t frame_buffer[SSD1306_H / 8][SSD1306_W] = { 0 };

#ifndef SSD1306_STR_SIZE
    #define SSD1306_STR_SIZE 25
#endif

const uint8_t SIN_LUT[] = {
    0, 4, 9, 13, 18, 22, 27, 31, 36, 40, 44, 49, 53, 58, 62, 66,
    71, 75, 79, 83, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 131,
    135, 139, 142, 146, 149, 153, 156, 159, 163, 166, 169, 172, 175, 178, 181, 183,
    186, 189, 191, 194, 196, 198, 201, 203, 205, 207, 209, 211, 212, 214, 216, 217,
    219, 220, 221, 223, 224, 225, 226, 227, 228, 228, 229, 230, 230, 231, 231, 231,
    232, 232, 232, 232, 232, 232, 232, 231, 231, 231, 230, 230, 229, 228, 228, 227,
    226, 225, 224, 223, 221, 220, 219, 217, 216, 214, 212, 211, 209, 207, 205, 203,
    201, 198, 196, 194, 191, 189, 186, 183, 181, 178, 175, 172, 169, 166, 163, 159,
    156, 153, 149, 146, 142, 139, 135, 131, 128, 124, 120, 116, 112, 108, 104, 100,
    96, 92, 88, 83, 79, 75, 71, 66, 62, 58, 53, 49, 44, 40, 36, 31,
    27, 22, 18, 13, 9, 4, 0
};

// Cosine table (just offset by 90 degrees from sine)
#define COS_LUT(angle) SIN_LUT[(angle + 90) % 360]


#define CHUNK_SIZE 32  		// Define your desired chunk size
#define SSD1306_PAGES       (SSD1306_H / 8) // 8 pages for 64 rows

#define SSD1306_W_LIMIT	SSD1306_W - 1
#define SSD1306_H_LIMIT	SSD1306_H - 1

typedef struct {
	uint8_t x;
	uint8_t y;
} T_Point;

// The SSD1306 frame buffer is divided into pages, each 8 pixels tall.
// At each x column, one byte stores that page's 8 vertical pixels.

void ssd1306_setwindow(uint8_t start_page, uint8_t end_page, uint8_t start_column, uint8_t end_column) {
	SSD1306_CMD(SSD1306_COLUMNADDR);
	SSD1306_CMD(start_column);   				// Column start address (0 = reset)
	SSD1306_CMD(end_column); 	// Column end address (127 = reset)

	SSD1306_CMD(SSD1306_PAGEADDR);
	SSD1306_CMD(start_page); 	// Page start address (0 = reset)
	SSD1306_CMD(end_page); 		// Page end address
}

//# display by pages
void ssd1306_displayArea(
	uint8_t start_page, uint8_t end_page,
	uint8_t col_start, uint8_t col_end
) {
	if (start_page >= SSD1306_PAGES || start_page > end_page || col_start >= SSD1306_W) return;
	if (end_page >= SSD1306_PAGES) end_page = SSD1306_PAGES - 1;
	if (col_end > SSD1306_W) col_end = SSD1306_W;
	if (col_start >= col_end) return;
	ssd1306_setwindow(start_page, end_page, col_start, col_end-1);

	for (uint8_t page = start_page; page <= end_page; ++page) {
		for (uint16_t chunk = col_start; chunk < col_end; chunk += CHUNK_SIZE) {
			uint16_t chunk_end = chunk + CHUNK_SIZE;
			if (chunk_end > col_end) chunk_end = col_end;
			SSD1306_DATA(&frame_buffer[page][chunk], chunk_end - chunk);
		}
	}
}

// Send the inclusive range of framebuffer pages across the full screen width.
static inline void ssd1306_displayPages(uint8_t start_page, uint8_t end_page) {
	ssd1306_displayArea(start_page, end_page, 0, SSD1306_W);
}

static uint8_t _render_invert = 0;

static inline void ssd1306_toggle_invert(void) {
	_render_invert ^= 1u;
}

// Copy up to SSD1306_STR_SIZE five-column glyphs into one framebuffer page.
void ssd1306_render_str_line(const char *str, uint8_t page, uint8_t column) {
	if (page >= SSD1306_PAGES) return;

	for (uint16_t i = 0; i < SSD1306_STR_SIZE; ++i) {
		uint16_t x = (uint16_t)column + i * 5u;
		if (x >= SSD1306_W) break;
		uint8_t char_index = 0;

		if (*str) {
			uint8_t c = (uint8_t)*str++;
			if (c >= 32 && c <= 126) char_index = c - 32;
		}
		uint8_t width = SSD1306_W - x < 5u ? SSD1306_W - x : 5u;
		memcpy(&frame_buffer[page][x], FONT_7x5[char_index], width);
	}
}

// SSD1306 8x8 build flash: ssd1306_drawstr 2816 bytes.
// SSD1306 8x8 build flash: ssd1306_render_str 2812 bytes.
//! Draw text into the frame buffer at pixel coordinates using the font's stored orientation.
void ssd1306_render_str(const char *str, uint8_t x, uint8_t y, const font_t *font) {
	uint8_t glyph_stride = font->row_major ? font->height : font->width;

	while (*str && x < SSD1306_W) {
		uint8_t c = (uint8_t)*str++;
		if (c < 32 || c > 126) c = ' ';
		const uint8_t *glyph = font->data + (uint16_t)(c - 32) * glyph_stride;

		if (font->row_major) {
			for (uint8_t row = 0; row < font->height; ++row) {
				uint16_t py = (uint16_t)y + row;
				if (py >= SSD1306_H) continue;
				uint8_t bits = glyph[row];

				for (uint8_t column = 0; column < font->width; ++column) {
					if (!(bits & (0x80u >> column))) continue;
					uint16_t px = (uint16_t)x + column;
					// if (px >= SSD1306_W) continue;

					uint8_t bitmask = 1u << (py & 0x07u);
					uint8_t *pixel = &frame_buffer[py >> 3][px];
					if (_render_invert)
						*pixel ^= bitmask;
					else
						*pixel |= bitmask;
				}
			}
		} else {
			for (uint8_t column = 0; column < font->width; ++column) {
				uint16_t px = (uint16_t)x + column;
				if (px >= SSD1306_W) continue;
				uint8_t bits = glyph[column];

				for (uint8_t row = 0; row < font->height; ++row) {
					if (!(bits & (1u << row))) continue;
					uint16_t py = (uint16_t)y + row;
					// if (py >= SSD1306_H) continue;

					uint8_t bitmask = 1u << (py & 0x07u);
					uint8_t *pixel = &frame_buffer[py >> 3][px];
					if (_render_invert)
						*pixel ^= bitmask;
					else
						*pixel |= bitmask;
				}
			}
		}
		x += font->width + font->spacing;
	}
}

//! Draw text with each font pixel expanded to a scale-by-scale square.
// |---|---:|
// | ssd1306_drawstr_sz | 2,844 bytes |
// | ssd1306_render_str_scaled | 2,872 bytes |

void ssd1306_render_str_scaled(const char *str, uint8_t x, uint8_t y,
	const font_t *font, uint8_t scale) {
	if (!scale || y >= SSD1306_H) return;
	uint8_t stride = font->row_major ? font->height : font->width;
	uint32_t cursor = x;

	while (*str && cursor < SSD1306_W) {
		uint8_t c = (uint8_t)*str++;
		if (c < 32 || c > 126) c = ' ';
		// Each character occupies one row block or one column block in the font.
		const uint8_t *glyph = font->data + (uint16_t)(c - 32) * stride;

		for (uint16_t row = 0; row < font->height; ++row) {
			// Scale the source row and stop once its first screen pixel is off-screen.
			uint32_t py = (uint32_t)y + row * scale;
			if (py >= SSD1306_H) break;

			for (uint16_t column = 0; column < font->width; ++column) {
				uint32_t px = cursor + column * scale;
				if (px >= SSD1306_W) break;
				// Row-major fonts store horizontal bits; column-major fonts store vertical bits.
				uint8_t bit = font->row_major ? (glyph[row] & (0x80u >> column))
												: (glyph[column] & (1u << row));
				if (!bit) continue;

				// Fill the scaled square, clipping its right and bottom edges.
				for (uint16_t dy = 0; dy < scale && py + dy < SSD1306_H; ++dy) {
					uint16_t screen_y = py + dy;
					// One framebuffer byte holds eight vertical pixels at this x position.
					uint8_t mask = 1u << (screen_y & 7u);
					uint8_t *pixels = &frame_buffer[screen_y >> 3][px];

					for (uint16_t dx = 0; dx < scale && px + dx < SSD1306_W; ++dx) {
						if (_render_invert)
							pixels[dx] ^= mask;
						else
							pixels[dx] |= mask;
					}
				}
			}
		}
		// Include the font's spacing when moving to the next character.
		cursor += (uint32_t)(font->width + font->spacing) * scale;
	}
}

// Blit row-major, MSB-first monochrome image data into the framebuffer.
// |---|---:|

void ssd1306_render_image(const uint8_t *image, uint8_t x, uint8_t y,
	uint8_t width, uint8_t height) {
	// Reject an empty destination, then clip the image to the screen once.
	if (!image || x >= SSD1306_W || y >= SSD1306_H) return;
	// Each source row stores one bit per pixel, rounded up to whole bytes.
	uint16_t stride = ((uint16_t)width + 7u) >> 3;
	uint16_t visible_width = SSD1306_W - x;
	if (visible_width > width) visible_width = width;
	uint16_t visible_height = SSD1306_H - y;
	if (visible_height > height) visible_height = height;

	for (uint16_t row = 0; row < visible_height; ++row) {
		uint8_t screen_y = y + row;
		// Framebuffer bytes hold 8 vertical pixels, so select the page and bit.
		uint8_t mask = 1u << (screen_y & 7u);
		uint8_t *dst = &frame_buffer[screen_y >> 3][x];
		const uint8_t *src = image + row * stride;

		// Read each packed source byte once and check its 8 MSB-first pixels.
		for (uint16_t byte = 0; byte < stride && byte * 8u < visible_width; ++byte) {
			uint8_t bits = src[byte];
			uint16_t col = byte * 8u;
			// Skip pixels beyond the clipped right edge; set lit pixels in the buffer.
			if (bits & 0x80u) dst[col] |= mask;
			if (visible_width > col + 1u && (bits & 0x40u)) dst[col + 1u] |= mask;
			if (visible_width > col + 2u && (bits & 0x20u)) dst[col + 2u] |= mask;
			if (visible_width > col + 3u && (bits & 0x10u)) dst[col + 3u] |= mask;
			if (visible_width > col + 4u && (bits & 0x08u)) dst[col + 4u] |= mask;
			if (visible_width > col + 5u && (bits & 0x04u)) dst[col + 5u] |= mask;
			if (visible_width > col + 6u && (bits & 0x02u)) dst[col + 6u] |= mask;
			if (visible_width > col + 7u && (bits & 0x01u)) dst[col + 7u] |= mask;
		}
	}
}

void ssd1306_setup() {
	memset(frame_buffer, 0, sizeof(frame_buffer));

	#if !defined(SSD1306_CUSTOM_INIT_ARRAY) || !SSD1306_CUSTOM_INIT_ARRAY
	uint8_t *cmd_list = (uint8_t *)ssd1306_init_array;
	while (*cmd_list != SSD1306_TERMINATE_CMDS) {
		if (SSD1306_CMD(*cmd_list++)) return;
	}
	#endif

	ssd1306_displayPages(0, SSD1306_PAGES - 1);
}

void ssd1306_render_pixel(T_Point point) {
	uint8_t *pixel = &frame_buffer[point.y >> 3][point.x];
	uint8_t mask = (uint8_t)(1u << (point.y & 7u));
	if (_render_invert) *pixel ^= mask;
	else *pixel |= mask;
}

void ssd1306_render_point(T_Point point) {
	if (point.x >= SSD1306_W || point.y >= SSD1306_H) return;
	ssd1306_render_pixel(point);
}


//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//! ***** Line Methods *****
//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// | Drawing code | Size |
// |---|---:|
// | ssd1306_drawPixel | 62 bytes |
// | ssd1306_drawFastVLine | 80 bytes |
// | ssd1306_drawFastHLine | 84 bytes |
// | Drawing total | 226 bytes |
// | _render_page_span | 134 bytes |
// | ssd1306_render_horLine | 160 bytes |
// | ssd1306_render_verLine | 78 bytes |
// | Prefill total | 372 bytes |


//! Apply a vertical pixel span across a horizontal range of the frame buffer.
static void _render_page_span(
	uint8_t y_min, uint8_t y_max,
	uint8_t x, uint8_t x_len
) {
	uint8_t first_page = y_min >> 3;
	uint8_t last_page = y_max >> 3;
	uint8_t first_bit = y_min & 0x07u;
	uint8_t last_bit = y_max & 0x07u;

	for (uint8_t page = first_page; page <= last_page; ++page) {
		uint8_t page_first_bit = (page == first_page) ? first_bit : 0;
		uint8_t page_last_bit = (page == last_page) ? last_bit : 7;
	    uint8_t page_mask = (uint8_t)(0xFFu << page_first_bit)
		                    & (uint8_t)(0xFFu >> (7u - page_last_bit));
		uint8_t *row_start = &frame_buffer[page][x];

		if (_render_invert) {
			for (uint8_t i = 0; i < x_len; ++i)
				row_start[i] ^= page_mask;
		} else {
			for (uint8_t i = 0; i < x_len; ++i)
				row_start[i] |= page_mask;
		}
	}
}


void ssd1306_render_horLine(
	uint8_t y, uint8_t x_min, uint8_t x_max, uint8_t thickness
) {
    // Validate coordinates
    if (y >= SSD1306_H) return;
	if (thickness == 0) return;

	// Clamp inclusive x bounds to the display.
	if (x_min >= SSD1306_W) x_min = SSD1306_W - 1;
	uint16_t x_end = (x_max >= SSD1306_W) ? SSD1306_W : (uint16_t)x_max + 1u;
	uint8_t x_len = x_end > x_min ? x_end - x_min : 0;

	if (thickness == 1) {
		uint8_t page = y >> 3;
		uint8_t mask = (uint8_t)(1u << (y & 7u));
		uint8_t *row_start = &frame_buffer[page][x_min];

		if (_render_invert) {
			for (uint8_t i = 0; i < x_len; ++i)
				row_start[i] ^= mask;
		} else {
			for (uint8_t i = 0; i < x_len; ++i)
				row_start[i] |= mask;
		}
		return;
	}

	uint8_t y_end = thickness > SSD1306_H - y ? SSD1306_H - 1
		                                    : y + thickness - 1u;

	// Combine the pixels within each 8-row page, then apply that mask across x.
	_render_page_span(y, y_end, x_min, x_len);
}

void ssd1306_render_verLine(
	uint8_t x, uint8_t y_min, uint8_t y_max, uint8_t thickness
) {
    // Validate coordinates
    if (x >= SSD1306_W) return;

	// Clamp to display bounds
	if (y_min >= SSD1306_H) y_min = SSD1306_H - 1;
	if (y_max >= SSD1306_H) y_max = SSD1306_H - 1;

    // Handle thickness
	if (thickness == 0) return;
	uint8_t remaining_width = SSD1306_W - x;
	uint8_t x_len = thickness > remaining_width ? remaining_width : thickness;

	// Combine pixels within each 8-row page and write once per page.
	_render_page_span(y_min, y_max, x, x_len);
}

void ssd1306_render_rect(
	T_Point p0, uint8_t width, uint8_t height, uint8_t fill
) {
	// Validate coordinates
	if (p0.x >= SSD1306_W || p0.y >= SSD1306_H) return;
	uint16_t x_end = p0.x + width;
	uint16_t y_end = p0.y + height;

	// Draw rectangle with optional fill

    if (fill) {
		for (uint16_t x_pos = p0.x; x_pos <= x_end; ++x_pos)
			ssd1306_render_verLine(x_pos, p0.y, y_end, 1);
    } else {
        // Outline only
        ssd1306_render_horLine(p0.y, p0.x, x_end, 1);     	// Top edge
        ssd1306_render_horLine(y_end, p0.x, x_end, 1);    	// Bottom edge
        ssd1306_render_verLine(p0.x, p0.y + 1, y_end - 1, 1); 		// Left edge
        ssd1306_render_verLine(x_end, p0.y + 1, y_end - 1, 1); 		// Right edge
    }
}

// |---|---:|---:|---:|
// | ssd1306_render_line() | 410 bytes |
// | ssd1306_drawLine() | 300 bytes |

//! (Bresenham's algorithm)
void ssd1306_render_line(T_Point p0, T_Point p1, uint8_t thickness) {
    // Bresenham's line algorithm
    int16_t dx = abs(p1.x - p0.x);
    int16_t dy = -abs(p1.y - p0.y);
    int16_t sx = p0.x < p1.x ? 1 : -1;
    int16_t sy = p0.y < p1.y ? 1 : -1;
    int16_t err = dx + dy;
    int16_t e2;

	uint8_t radius = thickness >> 1; 		// thickness/2 via bit shift

	while (1) {
		// Draw the pixel(s)
		if (thickness == 1) {
			// Fast path for single-pixel
			uint8_t page = p0.y >> 3;
			uint8_t mask = (uint8_t)(1u << (p0.y & 7u));
            if (_render_invert)
				frame_buffer[page][p0.x] ^= mask;
			else
				frame_buffer[page][p0.x] |= mask;
		} else {
			// Calculate bounds (branchless min/max)
			uint8_t x_start = p0.x > radius ? p0.x - radius : 0;
			uint8_t y_start = p0.y > radius ? p0.y - radius : 0;
			uint16_t x_stop = (uint16_t)p0.x + radius + 1u;
			uint16_t y_stop = (uint16_t)p0.y + radius + 1u;

			if (x_stop > SSD1306_W) x_stop = SSD1306_W;
			if (y_stop > SSD1306_H) y_stop = SSD1306_H;
			_render_page_span(y_start, (uint8_t)(y_stop - 1u),
			                  x_start, (uint8_t)(x_stop - x_start));
		}

		// Bresenham Advance
		if (p0.x == p1.x && p0.y == p1.y) break;
		e2 = err << 1; // e2 = 2*err via bit shift
		if (e2 >= dy) { err += dy; p0.x += sx; }
		if (e2 <= dx) { err += dx; p0.y += sy; }
	}
}

//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//! ***** Polygon Methods *****
//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void ssd1306_render_poly(T_Point *pts, uint8_t num_pts, uint8_t thickness) {
    if (num_pts < 3) return;  // Need at least 3 points for a polygon
    for (uint8_t i = 0; i < num_pts - 1; i++) {
        ssd1306_render_line(pts[i], pts[i+1], thickness);
    }
    ssd1306_render_line(pts[num_pts-1], pts[0], thickness);
}

//! optimized: compute filled polygon
void ssd1306_render_solid_poly(T_Point *pts, uint8_t num_pts) {
    // ===== [1] EDGE EXTRACTION =====
    struct Edge {
        uint8_t y_start, y_end;
        int16_t x_start, dx_dy;
    } edges[num_pts];

    uint8_t edge_count = 0;
    uint8_t y_min = 255, y_max = 0;

    // Build edge table and find Y bounds
    for (uint8_t i = 0, j = num_pts-1; i < num_pts; j = i++) {
        // Skip horizontal edges (don't affect filling)
        if (pts[i].y == pts[j].y) continue;

        // Determine edge direction
        uint8_t y0, y1;
        int16_t x0;
        if (pts[i].y < pts[j].y) {
            y0 = pts[i].y; y1 = pts[j].y;
            x0 = pts[i].x;
        } else {
            y0 = pts[j].y; y1 = pts[i].y;
            x0 = pts[j].x;
        }

        // Update global Y bounds
        y_min = y0 < y_min ? y0 : y_min;
        y_max = y1 > y_max ? y1 : y_max;

        // Store edge (dx/dy as fixed-point 8.8)
        edges[edge_count++] = (struct Edge){
            .y_start = y0,
            .y_end = y1,
            .x_start = x0 << 8,
            .dx_dy = ((pts[j].x - pts[i].x) << 8) / (pts[j].y - pts[i].y)
        };
    }

    // ===== [2] SCANLINE PROCESSING =====
    for (uint8_t y = y_min; y <= y_max; y++) {
        uint8_t x_list[8];  // Supports 4 edge crossings (99% of cases)
        uint8_t x_count = 0;

        // Collect active edges
        for (uint8_t e = 0; e < edge_count; e++) {
            if (y >= edges[e].y_start && y < edges[e].y_end) {
                x_list[x_count++] = edges[e].x_start >> 8;
                edges[e].x_start += edges[e].dx_dy;  // Step X
            }
        }

        // Insertion sort (optimal for small N)
        for (uint8_t i = 1; i < x_count; i++) {
            uint8_t val = x_list[i];
            int8_t j = i-1;
            while (j >= 0 && x_list[j] > val) {
                x_list[j+1] = x_list[j];
                j--;
            }
            x_list[j+1] = val;
        }

        // Fill between pairs (with bounds checking)
        for (uint8_t i = 0; i+1 < x_count; i += 2) {
            uint8_t x1 = x_list[i] < SSD1306_W ? x_list[i] : SSD1306_W-1;
            uint8_t x2 = x_list[i+1] < SSD1306_W ? x_list[i+1] : SSD1306_W-1;
            if (x1 < x2) ssd1306_render_horLine(y, x1, x2, 1);
        }
    }
}

//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//! ***** Circle Methods *****
//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// |---|---|---:|
// | ssd1306_render_circle() | ssd1306_render_circle + ssd1306_render_horLine | 438 B |
// | SSD1306 circles | drawCircle + fillCircle + drawPixel + drawFastHLine | 430 B |

//! compute circle (Bresenham's algorithm)
void ssd1306_render_circle(
	T_Point p0, uint8_t radius, uint8_t fill
) {
	if (fill) {
		if (p0.x >= SSD1306_W || p0.y >= SSD1306_H) return;
	} else {
		if (p0.x < radius || p0.y < radius
			|| (uint16_t)p0.x + radius >= SSD1306_W
			|| (uint16_t)p0.y + radius >= SSD1306_H) return;
	}

    int16_t x = -radius;
    int16_t y = 0;
    int16_t err = 2 - 2 * radius;
	int16_t e2;

    do {
        // Calculate endpoints with clamping
        uint8_t x_start 	= p0.x + x;
        uint8_t x_end   	= p0.x - x;
        uint8_t y_top   	= p0.y - y;
        uint8_t y_bottom 	= p0.y + y;

		if (fill) {
            // Draw filled horizontal lines (top and bottom halves)
            ssd1306_render_horLine(y_top, x_start, x_end, 1);     // Top half
            ssd1306_render_horLine(y_bottom, x_start, x_end, 1);  // Bottom half
		} else {
			// Plot the two endpoints of each horizontal span.
			uint8_t top_page = y_top >> 3;
			uint8_t top_mask = (uint8_t)(1u << (y_top & 7u));
			uint8_t bottom_page = y_bottom >> 3;
			uint8_t bottom_mask = (uint8_t)(1u << (y_bottom & 7u));

			if (_render_invert) {
				frame_buffer[top_page][x_start] ^= top_mask;
				frame_buffer[top_page][x_end] ^= top_mask;
				frame_buffer[bottom_page][x_start] ^= bottom_mask;
				frame_buffer[bottom_page][x_end] ^= bottom_mask;
			} else {
				frame_buffer[top_page][x_start] |= top_mask;
				frame_buffer[top_page][x_end] |= top_mask;
				frame_buffer[bottom_page][x_start] |= bottom_mask;
				frame_buffer[bottom_page][x_end] |= bottom_mask;
			}
		}

        // Update Bresenham error
		e2 = err;
        if (e2 <= y) {
            err += ++y * 2 + 1;
            if (-x == y && e2 <= x) e2 = 0;
        }
        if (e2 > x) err += ++x * 2 + 1;
    } while (x <= 0);
}



// Helper function to get circle point using SIN_LUT
static void get_circle_point(
	T_Point center, uint8_t radius, int16_t angle,
	int16_t *x, int16_t *y
) {
    if (angle == 360) angle = 0;
    uint8_t quadrant = angle / 90;
    uint8_t reduced_angle = angle % 90;
    uint8_t sin_val = SIN_LUT[reduced_angle];
    uint8_t cos_val = SIN_LUT[90 - reduced_angle];

    switch (quadrant) {
        case 0: *x = center.x + (radius * cos_val) / 255;
                *y = center.y - (radius * sin_val) / 255;
                break;
        case 1: *x = center.x - (radius * sin_val) / 255;
                *y = center.y - (radius * cos_val) / 255;
                break;
        case 2: *x = center.x - (radius * cos_val) / 255;
                *y = center.y + (radius * sin_val) / 255;
                break;
        case 3: *x = center.x + (radius * sin_val) / 255;
                *y = center.y + (radius * cos_val) / 255;
                break;
    }
}


void ssd1306_render_ring(
	T_Point p0, uint8_t radius, uint8_t thickness,
	int16_t start_angle, int16_t end_angle
) {
    // Early exit if center is off-screen or radius is 0
    if ((p0.x >= SSD1306_W) | (p0.y >= SSD1306_H) | (radius == 0)) return;
    // Precompute display bounds and thickness
    uint8_t inner_r = (thickness >= radius) ? 1 : (radius - thickness);
    uint8_t* const frame_base = &frame_buffer[0][0];

    // Draw concentric circles (outer to inner)
    for (uint8_t r = radius; r >= inner_r; r--) {
        int16_t x = -r;
        int16_t y = 0;
        int16_t err = 2 - 2 * r;

        do {
            // Calculate and clamp coordinates with 1-pixel extension
            int16_t x_start = p0.x + x;
            int16_t x_end   = p0.x - x;
            int16_t y_top    = p0.y - y;
            int16_t y_bottom = p0.y + y;

			// //! Fill out 2 extra pixels besides x to make ensure no missing
			// //! pixel when drawing the circles
			int16_t x_left  = x_start - 1;
            int16_t x_right = x_end + 1;
			uint8_t left_valid = x_left >= 0 && x_left < SSD1306_W;
			uint8_t start_valid = x_start >= 0 && x_start < SSD1306_W;
			uint8_t end_valid = x_end >= 0 && x_end < SSD1306_W;
			uint8_t right_valid = x_right >= 0 && x_right < SSD1306_W;

            // Skip pixels outside valid screen bounds
            if (y_top >= 0 && y_top < SSD1306_H) {
                uint8_t* row_t = frame_base + ((y_top >> 3) * SSD1306_W);
                uint8_t mask_t = (uint8_t)(1u << (y_top & 7));

                if (left_valid)     row_t[x_left]  |= mask_t;
                if (start_valid)    row_t[x_start] |= mask_t;
                if (end_valid) 	    row_t[x_end]   |= mask_t;
                if (right_valid)    row_t[x_right] |= mask_t;
            }

            if (y_bottom >= 0 && y_bottom < SSD1306_H) {
                uint8_t* row_b = frame_base + ((y_bottom >> 3) * SSD1306_W);
                uint8_t mask_b = (uint8_t)(1u << (y_bottom & 7));

                if (left_valid) 	row_b[x_left]  |= mask_b;
                if (start_valid)    row_b[x_start] |= mask_b;
                if (end_valid) 	    row_b[x_end]   |= mask_b;
                if (right_valid)    row_b[x_right] |= mask_b;
            }

            // Optimized Bresenham step
            int16_t e2 = err;
            if (e2 <= y) err += ++y * 2 + 1;  // y*2+1
            if (e2 > x)  err += ++x * 2 + 1;  // x*2+1
        } while (x <= 0);
    }
}

// | Renderer | Renderer function | Helpers | Total |
// |---|---:|---|---:|
// | ssd1306_render_pie | 636 B | get_circle_point 266 B, SIN_LUT 167 B, ssd1306_render_horLine 100 B | 1,169 B |
// | ssd1306_render_pie_fast | 740 B | Shared helpers above, _pie_edge_init 168 B, _pie_fast_span 116 B | 1,557 B |

void ssd1306_render_pie(T_Point center, uint8_t radius, int16_t start_angle, int16_t end_angle) {
    // Draw center point directly in the framebuffer.
    if (center.x < SSD1306_W && center.y < SSD1306_H) {
        uint8_t page = center.y >> 3;
        uint8_t mask = 1u << (center.y & 7u);
        if (_render_invert)
            frame_buffer[page][center.x] ^= mask;
        else
            frame_buffer[page][center.x] |= mask;
    }

    // Special case: full circle
    if (start_angle == end_angle) {
        ssd1306_render_circle(center, radius, 1); // Fill entire circle
        return;
    }

    // Angle stepping (adaptive based on radius)
    uint8_t step = (radius > 40) ? 2 : 1;

    // Draw radial lines
    int16_t angle = start_angle;
	int16_t x, y;
	int16_t last_x = 0, last_y = 0;
	uint8_t have_last = 0;
	uint8_t skip_duplicates = !_render_invert;

    while (1) {
        // Calculate edge point
        get_circle_point(center, radius, angle, &x, &y);

        // Adjacent angles can produce the same endpoint and the same line.
		if (!skip_duplicates || !have_last || x != last_x || y != last_y) {
			T_Point p0 = { x: center.x, y: center.y };
			T_Point p1 = { x: x, y: y };
			ssd1306_render_line(p0, p1, 1);
			last_x = x;
			last_y = y;
			have_last = 1;
		}

        if (angle == end_angle) break;

        // Move to next angle
        angle = (angle + step) % 360;
        // Handle wrap-around
        if (start_angle > end_angle && angle < start_angle && angle > end_angle) break;
    }
}

// Tracks a slanted pie edge's crossing as the renderer advances column by column.
// The remainder carries fractional movement so each column update avoids division.
typedef struct {
    int16_t a, numerator, denominator;
	int16_t step, remainder, error;
    int32_t position;
} T_Pie_Edge;

// Set up a pie edge's starting position, whole-number step, and fractional remainder.
static T_Pie_Edge _pie_edge_init(int16_t a, int16_t numerator) {
    T_Pie_Edge edge = { .a = a, .numerator = numerator };
    if (a != 0) {
        edge.denominator = a > 0 ? a : -a;
        int16_t n = a > 0 ? numerator : -numerator;
        edge.step = n / edge.denominator;
        edge.remainder = n - edge.step * edge.denominator;

        if (edge.remainder < 0) {
            --edge.step;
            edge.remainder += edge.denominator;
        }
    }
    return edge;
}

static void _pie_edge_advance(T_Pie_Edge *edge) {
    // Move the edge one column: add its whole-number step, then use the remainder
    // to decide whether the crossing also advances by one pixel.
    if (edge->a == 0) return;
    edge->position += edge->step;
    edge->error += edge->remainder;

    if (edge->error >= edge->denominator) {
        edge->error -= edge->denominator;
        ++edge->position;
    }
}

// Cut off the part of this circle column outside one slanted pie side.
// Return false if the cut leaves no pixels in the pie.
static uint8_t _pie_clip_edge(
	int16_t *lo, int16_t *hi,
	const T_Pie_Edge *edge, int16_t up, uint8_t center_x
) {
    if (edge->a == 0)
		return -(int32_t)edge->numerator * up >= 0;

    // Mirror the accumulated boundary for the bottom half, preserving exact rounding.
    int32_t floor_x = up < 0 ? -edge->position - (edge->error != 0) : edge->position;
    int32_t bound = center_x + floor_x;

    if (edge->a > 0) {
        bound += edge->error != 0; // ceil for the inclusive lower bound
        if (bound > *hi) return 0;
        if (bound > *lo) *lo = bound;
    } else {
        if (bound < *lo) return 0;
        if (bound < *hi) *hi = bound;
    }
    return *lo <= *hi;
}


// Clip a row to a*x + b >= 0 with inclusive integer boundaries.
// Write one vertical pixel span as masked page bytes in the framebuffer.
static inline void _pie_fast_span(uint8_t x, uint8_t lo, uint8_t hi) {
    // SSD1306 pages store 8 vertical pixels per byte; mask partial end pages.
    uint8_t first = lo >> 3, last = hi >> 3;
    uint8_t first_mask = (uint8_t)(0xFFu << (lo & 7u));
    uint8_t last_mask = (uint8_t)(0xFFu >> (7u - (hi & 7u)));

    if (first == last) {
        uint8_t mask = first_mask & last_mask;
        if (_render_invert) frame_buffer[first][x] ^= mask;
        else frame_buffer[first][x] |= mask;
    // Edge masks preserve pixels outside the span; middle pages are fully covered.
    } else if (_render_invert) {
        // XOR toggles covered pixels for invert mode.
        frame_buffer[first][x] ^= first_mask;
        for (uint8_t page = first + 1; page < last; ++page)
			frame_buffer[page][x] ^= 0xFFu; // Toggle all 8 pixels in this page.
        frame_buffer[last][x] ^= last_mask;
    } else {
        // OR turns covered pixels on without changing pixels outside the masks.
        frame_buffer[first][x] |= first_mask;
        for (uint8_t page = first + 1; page < last; ++page)
			frame_buffer[page][x] = 0xFFu;
        frame_buffer[last][x] |= last_mask;
    }
}

void ssd1306_render_pie_fast(T_Point center, uint8_t radius, int16_t start_angle, int16_t end_angle) {
    if (radius == 0) return;
    if (start_angle < 0 || start_angle > 360
        || end_angle < 0 || end_angle > 360) return;
    if (start_angle >= end_angle) return;

    int16_t sweep = end_angle - start_angle;
    if (sweep == 0) return;

    int16_t start_x, start_y, end_x, end_y;
    get_circle_point(center, radius, start_angle, &start_x, &start_y);
    get_circle_point(center, radius, end_angle, &end_x, &end_y);
    int32_t circle_error = 0; // extent*extent + dx*dx - radius*radius
    int16_t extent = radius;
    T_Pie_Edge start_edge = _pie_edge_init(center.x - start_x, center.y - start_y);
    T_Pie_Edge end_edge = _pie_edge_init(end_x - center.x, end_y - center.y);

    // Scan columns outward; each span combines vertical pixels into page bytes.
    for (int16_t dx = 0; dx <= radius; ++dx) {
        // Decreasing extent changes its square by -(2*extent - 1).
        while (circle_error > 0) {
            circle_error -= 2 * extent - 1;
            --extent;
        }

        int16_t y_min = (int16_t)center.y - extent;
        int16_t y_max = (int16_t)center.y + extent;
        if (y_min < 0) y_min = 0;
        if (y_max >= SSD1306_H) y_max = SSD1306_H - 1;
        if (y_min > y_max) continue;

        for (uint8_t half = 0; half < (dx == 0 ? 1 : 2); ++half) {
            int16_t right = half == 0 ? dx : -dx;
            int16_t x = (int16_t)center.x + right;
            if (x < 0 || x >= SSD1306_W) continue;

            // Clip to the incrementally tracked angle boundaries.
            int16_t lo1 = y_min, hi1 = y_max;
            uint8_t first = _pie_clip_edge(&lo1, &hi1, &start_edge, right, center.y);

            if (sweep <= 180) {
                if (first && _pie_clip_edge(&lo1, &hi1, &end_edge, right, center.y))
                    _pie_fast_span((uint8_t)x, (uint8_t)lo1, (uint8_t)hi1);
            } else {
                int16_t lo2 = y_min, hi2 = y_max;
                uint8_t second = _pie_clip_edge(&lo2, &hi2, &end_edge, right, center.y);
                if (first && second && lo1 <= hi2 + 1 && lo2 <= hi1 + 1) {
                    if (lo2 < lo1) lo1 = lo2;
                    if (hi2 > hi1) hi1 = hi2;
                    second = 0;
                }
                if (first) _pie_fast_span((uint8_t)x, (uint8_t)lo1, (uint8_t)hi1);
                if (second) _pie_fast_span((uint8_t)x, (uint8_t)lo2, (uint8_t)hi2);
            }
        }
        // Advancing dx changes its square by 2*dx + 1.
        circle_error += 2 * dx + 1;
        _pie_edge_advance(&start_edge);
        _pie_edge_advance(&end_edge);
    }
}

#endif // _MODSSD1306_H
