/*
 * Single-File-Header for using SPI OLED
 * 05-05-2023 E. Brombaugh
 */

#ifndef _SSD1306_H
#define _SSD1306_H

#include <stdint.h>
#include <string.h>
#include "word_font.h"

// comfortable packet size for this OLED
#define SSD1306_PSZ 32

#if defined (SSD1306_CUSTOM)
// Let the caller configure the OLED.
#else
// characteristics of each type
#if !defined (SSD1306_64X32) && !defined (SSD1306_128X32) && !defined (SSD1306_128X64) && !defined (SH1107_128x128) && !(defined(SSD1306_W) && defined(SSD1306_H) && defined(SSD1306_OFFSET) )
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


uint8_t ssd1306_cmd(uint8_t cmd);
uint8_t ssd1306_data(uint8_t *data, int sz);

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
uint8_t ssd1306_buffer[SSD1306_W * SSD1306_H / 8];

/* set the buffer to a color */
void ssd1306_setbuf(uint8_t color) {
	memset(ssd1306_buffer, color ? 0xFF : 0x00, sizeof(ssd1306_buffer));
}

/* Implemented by fun_ssd1306.h; sends only the requested framebuffer area. */
void ssd1306_displayArea(uint8_t start_page, uint8_t end_page,
	uint8_t col_start, uint8_t col_end);

/* initialize I2C and OLED  */
uint8_t ssd1306_init(void) {
	// pulse reset
	// ssd1306_rst();
	ssd1306_setbuf(0);

	// initialize OLED
	#if !defined(SSD1306_CUSTOM_INIT_ARRAY) || !SSD1306_CUSTOM_INIT_ARRAY
		uint8_t *cmd_list = (uint8_t *)ssd1306_init_array;

		while(*cmd_list != SSD1306_TERMINATE_CMDS) {
			if(ssd1306_cmd(*cmd_list++)) return 1;
		}

		ssd1306_displayArea(0, SSD1306_H / 8 - 1, 0, SSD1306_W);
	#endif

	return 0;
}

#endif

