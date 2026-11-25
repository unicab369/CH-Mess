#ifndef _MODSSD1306_H
#define _MODSSD1306_H

#include "ch32fun.h"

#define SSD1306_128X64
#include "ssd1306.h"

/* send OLED command byte */
uint8_t ssd1306_cmd(uint8_t cmd);

/* send OLED data packet (up to 32 bytes) */
uint8_t ssd1306_data(uint8_t *data, int sz);

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

#define M_PI 3.14

#ifdef SSD1306_SHARED_FRAME_BUFFER
#define frame_buffer ((uint8_t (*)[SSD1306_W])ssd1306_buffer)
#else
uint8_t frame_buffer[SSD1306_PAGES][SSD1306_W] = { 0 };
#endif

typedef struct {
	uint8_t x;
	uint8_t y;
} M_Point;

typedef struct {
	uint8_t page;
	uint8_t bitmask;
} M_Page_Mask;

// The SSD1306 frame buffer is divided into pages, each 8 pixels tall.
// At each x column, one byte stores that page's 8 vertical pixels.
// page_masks acts as a lookup translator from a pixel's y position to its page and bit.

// For each pixel's Y position, page_masks tells the code:
// - Which page (group of 8 vertical pixels) contains it.
// - Which bit within that page's byte to set.
M_Page_Mask page_masks[SSD1306_H];

void precompute_page_masks() {
	for (uint8_t y = 0; y < SSD1306_H; y++) {
		page_masks[y].page       = y >> 3;             // (y / 8)
		// Pick one vertical pixel position in this 8-pixel page. Example: offset 2:
		//   screen y:  0  1 [2] 3  4  5  6  7   (top to bottom within the page)
		//   bit:       0  1  2  3  4  5  6  7
		//   mask:      00000100
		// y % 8 gives this position within the page; e.g. y = 10 -> page 1, position 2.
		page_masks[y].bitmask    = 1 << (y & 0x07);    // same as 1 << (y % 8)
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

void render_str(
  const char *str, uint8_t page, uint8_t column
) {
	ssd1306_setwindow(page, page, 0, SSD1306_W_LIMIT); // Set the window to the current page

	for (int i=0; i < SSD1306_STR_SIZE; i++) {
		if (*str) {
			uint8_t char_index = *str - 32; // Adjust for ASCII offset
			ssd1306_data((uint8_t *)FONT_7x5[char_index], 5); // Send font data
			str++;
		} else {
			ssd1306_data((uint8_t *)FONT_7x5[0], 5); // Send space character if no more characters
		}
	}
}

//! display area
void ssd1306_displayArea(
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

//! display the entire screen
void ssd1306_displayFrame() {
	ssd1306_displayArea(0, 7, 0, SSD1306_W);
}

static uint8_t _render_invert = 0;

static inline void render_toggle_invert(void) {
	_render_invert ^= 1u;
}

//! compute pixel
void render_pixel(uint8_t x, uint8_t y) {
    if (x >= SSD1306_W || y >= SSD1306_H) return; // Skip if out of bounds
    // Look up the precomputed page and bit mask that correspond to this y coordinate.
    M_Page_Mask mask = page_masks[y];
    // mask.page selects the 8-pixel vertical page for y; mask.bitmask selects y's bit within it.
    if (_render_invert)
        frame_buffer[mask.page][x] ^= mask.bitmask;
    else
        frame_buffer[mask.page][x] |= mask.bitmask;
}

void ssd1306_setup() {
	precompute_page_masks();
	ssd1306_init();

	//# Clear the frame buffer
	memset(frame_buffer, 0, SSD1306_PAGES * SSD1306_W);
	ssd1306_displayFrame();
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
// | render_horLine | 160 bytes |
// | render_verLine | 78 bytes |
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


void render_horLine(
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
		M_Page_Mask mask = page_masks[y];
		uint8_t *row_start = &frame_buffer[mask.page][x_min];

		if (_render_invert) {
			for (uint8_t i = 0; i < x_len; ++i)
				row_start[i] ^= mask.bitmask;
		} else {
			for (uint8_t i = 0; i < x_len; ++i)
				row_start[i] |= mask.bitmask;
		}
		return;
	}

	uint8_t y_end = thickness > SSD1306_H - y ? SSD1306_H - 1
		                                    : y + thickness - 1u;

	// Combine the pixels within each 8-row page, then apply that mask across x.
	_render_page_span(y, y_end, x_min, x_len);
}

void render_verLine(
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

void render_rect(
	M_Point p0, uint8_t width, uint8_t height, uint8_t fill
) {
	// Validate coordinates
	if (p0.x >= SSD1306_W || p0.y >= SSD1306_H) return;
	uint16_t x_end = p0.x + width;
	uint16_t y_end = p0.y + height;

	// Draw rectangle with optional fill

    if (fill) {
		for (uint16_t x_pos = p0.x; x_pos <= x_end; ++x_pos)
			render_verLine(x_pos, p0.y, y_end, 1);
    } else {
        // Outline only
        render_horLine(p0.y, p0.x, x_end, 1);     	// Top edge
        render_horLine(y_end, p0.x, x_end, 1);    	// Bottom edge
        render_verLine(p0.x, p0.y + 1, y_end - 1, 1); 		// Left edge
        render_verLine(x_end, p0.y + 1, y_end - 1, 1); 		// Right edge
    }
}

// |---|---:|---:|---:|
// | render_line() | 410 bytes |
// | ssd1306_drawLine() | 300 bytes |

//! (Bresenham's algorithm)
void render_line(M_Point p0, M_Point p1, uint8_t thickness) {
    // Bresenham's line algorithm
    int16_t dx = abs(p1.x - p0.x);
    int16_t dy = -abs(p1.y - p0.y);
    int16_t sx = p0.x < p1.x ? 1 : -1;
    int16_t sy = p0.y < p1.y ? 1 : -1;
    int16_t err = dx + dy;
    int16_t e2;

	// Precompute these before the loop:
	uint8_t *fb_base = &frame_buffer[0][0];
	uint8_t radius = thickness >> 1; 		// thickness/2 via bit shift

	while (1) {
		// Draw the pixel(s)
		if (thickness == 1) {
			// Fast path for single-pixel
            M_Page_Mask mask = page_masks[p0.y];
            if (_render_invert)
				frame_buffer[mask.page][p0.x] ^= mask.bitmask;
			else
				frame_buffer[mask.page][p0.x] |= mask.bitmask;
		} else {
			// Calculate bounds (branchless min/max)
			uint8_t x_start = p0.x > radius ? p0.x - radius : 0;
			uint8_t y_start = p0.y > radius ? p0.y - radius : 0;
			uint16_t x_stop = (uint16_t)p0.x + radius + 1u;
			uint16_t y_stop = (uint16_t)p0.y + radius + 1u;

			if (x_stop > SSD1306_W) x_stop = SSD1306_W;
			if (y_stop > SSD1306_H) y_stop = SSD1306_H;
			uint8_t width = x_stop - x_start;
			uint8_t *row_start = fb_base + x_start;

			// Optimized row filling
			for (uint8_t y = y_start; y < y_stop; y++) {
				// Look up the precomputed page and bit mask that correspond to this y coordinate.
				M_Page_Mask mask = page_masks[y];
				uint8_t *row = row_start + (mask.page * SSD1306_W);

				if (_render_invert) {
					for (uint8_t i = 0; i < width; ++i)
						row[i] ^= mask.bitmask;
				} else {
					for (uint8_t i = 0; i < width; ++i)
						row[i] |= mask.bitmask;
				}
			}
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

void render_poly(M_Point *pts, uint8_t num_pts, uint8_t thickness) {
    if (num_pts < 3) return;  // Need at least 3 points for a polygon
    for (uint8_t i = 0; i < num_pts - 1; i++) {
        render_line(pts[i], pts[i+1], thickness);
    }
    render_line(pts[num_pts-1], pts[0], thickness);
}

//! optimized: compute filled polygon
void render_solid_poly(M_Point *pts, uint8_t num_pts) {
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
            if (x1 < x2) render_horLine(y, x1, x2, 1);
        }
    }
}

//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//! ***** Circle Methods *****
//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// |---|---|---:|
// | render_circle() | render_circle + render_horLine | 438 B |
// | SSD1306 circles | drawCircle + fillCircle + drawPixel + drawFastHLine | 430 B |

//! compute circle (Bresenham's algorithm)
void render_circle(
	M_Point p0, uint8_t radius, uint8_t fill
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
            render_horLine(y_top, x_start, x_end, 1);     // Top half
            render_horLine(y_bottom, x_start, x_end, 1);  // Bottom half
		} else {
			// Plot the two endpoints of each horizontal span.
			M_Page_Mask top = page_masks[y_top];
			M_Page_Mask bottom = page_masks[y_bottom];

			if (_render_invert) {
				frame_buffer[top.page][x_start] ^= top.bitmask;
				frame_buffer[top.page][x_end] ^= top.bitmask;
				frame_buffer[bottom.page][x_start] ^= bottom.bitmask;
				frame_buffer[bottom.page][x_end] ^= bottom.bitmask;
			} else {
				frame_buffer[top.page][x_start] |= top.bitmask;
				frame_buffer[top.page][x_end] |= top.bitmask;
				frame_buffer[bottom.page][x_start] |= bottom.bitmask;
				frame_buffer[bottom.page][x_end] |= bottom.bitmask;
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
	M_Point center, uint8_t radius, int16_t angle,
	int16_t *x, int16_t *y
) {
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


void render_ring(
	M_Point p0, uint8_t radius, uint8_t thickness,
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
                M_Page_Mask mask_t = page_masks[y_top];
                uint8_t* row_t = frame_base + (mask_t.page * SSD1306_W);

                if (left_valid)     row_t[x_left]  |= mask_t.bitmask;
                if (start_valid)    row_t[x_start] |= mask_t.bitmask;
                if (end_valid) 	    row_t[x_end]   |= mask_t.bitmask;
                if (right_valid)    row_t[x_right] |= mask_t.bitmask;
            }

            if (y_bottom >= 0 && y_bottom < SSD1306_H) {
                M_Page_Mask mask_b = page_masks[y_bottom];
                uint8_t* row_b = frame_base + (mask_b.page * SSD1306_W);

                if (left_valid) 	row_b[x_left]  |= mask_b.bitmask;
                if (start_valid)    row_b[x_start] |= mask_b.bitmask;
                if (end_valid) 	    row_b[x_end]   |= mask_b.bitmask;
                if (right_valid)    row_b[x_right] |= mask_b.bitmask;
            }

            // Optimized Bresenham step
            int16_t e2 = err;
            if (e2 <= y) err += ++y * 2 + 1;  // y*2+1
            if (e2 > x)  err += ++x * 2 + 1;  // x*2+1
        } while (x <= 0);
    }
}

void render_pie(M_Point center, uint8_t radius, int16_t start_angle, int16_t end_angle) {
    // Draw center point
    render_pixel(center.x, center.y);

    // Special case: full circle
    if (start_angle == end_angle) {
        render_circle(center, radius, 1); // Fill entire circle
        return;
    }

    // Angle stepping (adaptive based on radius)
    uint8_t step = (radius > 40) ? 2 : 1;

    // Draw radial lines
    int16_t angle = start_angle;
	int16_t x, y;

    while (1) {
        // Calculate edge point
        get_circle_point(center, radius, angle, &x, &y);

        // Draw line from center to edge
		M_Point p0 = { x: center.x, y: center.y };
		M_Point p1 = { x: x, y: y };
		render_line(p0, p1, 1);

        // Break conditions
        if (angle == end_angle) break;

        // Move to next angle
        angle = (angle + step) % 360;

        // Handle wrap-around
        if (start_angle > end_angle && angle < start_angle && angle > end_angle) break;
    }
}


//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//! ***** Tests *****
//! %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


int ssd1306_test2() {
	for(uint8_t mode=0; mode<(SSD1306_H>32?9:8); mode++) {
		// clear buffer for next mode
		ssd1306_setbuf(0);

		// switch(mode) {
		//    case 0:
		//       printf("buffer fill with binary\n\r");
		//       for(int i=0;i<sizeof(ssd1306_buffer);i++)
		//          ssd1306_buffer[i] = i;
		//       break;
			
		//    case 1:
		//       printf("pixel plots\n\r");
				// for(int i=0;i<SSD1306_W;i++)
				// {
				// 	ssd1306_drawPixel(i, i/(SSD1306_W/SSD1306_H), 1);
				// 	ssd1306_drawPixel(i, SSD1306_H-1-(i/(SSD1306_W/SSD1306_H)), 1);
				// }
		//       break;
			
		//    case 2:
		//       {
		//          printf("Line plots\n\r");
				// uint8_t y= 0;
				// for(uint8_t x=0;x<SSD1306_W;x+=16)
				// {
				// 	y+= SSD1306_H/8;
				// }
		//       }
		//       break;
				
		//    case 4:
		//       printf("Image\n\r");
		//       ssd1306_drawImage(0, 0, bomb_i_stripped, 32, 32, 0);
		//       break;
		//    case 5:
		//       printf("Unscaled Text\n\r");
				ssd1306_drawstr(0,0, "This is a test", 1);
				ssd1306_drawstr(0,8, "of the emergency", 1);
				ssd1306_drawstr(0,16,"broadcasting", 1);
				ssd1306_drawstr(0,24,"system.",1);
				if(SSD1306_H>32) {
					ssd1306_drawstr(0,32, "Lorem ipsum", 1);
					ssd1306_drawstr(0,40, "dolor sit amet,", 1);
					ssd1306_drawstr(0,48,"consectetur", 1);
					ssd1306_drawstr(0,56,"adipiscing",1);
				}
				ssd1306_xorrect(SSD1306_W/2, 0, SSD1306_W/2, SSD1306_W);
		//       break;
				
		//    case 6:
		//       printf("Scaled Text 1, 2\n\r");
				// ssd1306_drawstr_sz(0,0, "sz 8x8", 1, fontsize_8x8);
				// ssd1306_drawstr_sz(0,16, "16x16", 1, fontsize_16x16);
		//       break;
			
		//    case 7:
		//       printf("Scaled Text 4\n\r");
		//       ssd1306_drawstr_sz(0,0, "32x32", 1, fontsize_32x32);
		//       break;
			
		//    case 8:
		//       // printf("Scaled Text 8\n\r");
		//       ssd1306_drawstr_sz(0,0, "64", 1, fontsize_64x64);
		//       break;

		//    default:
		//       break;
		// }

		ssd1306_refresh();
	}
}


#endif // _MODSSD1306_H
