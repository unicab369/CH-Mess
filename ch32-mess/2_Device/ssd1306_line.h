#include "modSSD1306.h"
#include "ch32fun.h"

//! compute_fastHorLine
void prefill_fastHorLine(uint8_t y, uint8_t x0, uint8_t x1) {
    if (y >= SSD1306_H) return;
    
	// Clamp x-coordinates
	if (x0 >= SSD1306_W) x0 = SSD1306_W_LIMIT;
	if (x1 >= SSD1306_W) x1 = SSD1306_W_LIMIT;

    M_Page_Mask mask = page_masks[y];
    for (uint8_t x = x0; x <= x1; x++) {
        frame_buffer[mask.page][x] |= mask.bitmask;
    }
}

//! compute horizontal line
void prefill_horLine(
	uint8_t y, Limit x_limit, uint8_t thickness, uint8_t mirror
) {
    // Validate coordinates
    if (y >= SSD1306_H) return;

	// Clamp to display bounds
    if (x_limit.l0 >= SSD1306_W) x_limit.l0 = SSD1306_W_LIMIT;
    if (x_limit.l1 >= SSD1306_W) x_limit.l1 = SSD1306_W_LIMIT;
    
	// Handle mirroring
	if (mirror) {
		x_limit.l0 = SSD1306_W_LIMIT - x_limit.l0;
		x_limit.l1 = SSD1306_W_LIMIT - x_limit.l1;
	}

	// Ensure x1 <= x2 (swap if needed)
	if (x_limit.l0 > x_limit.l1) {
		uint8_t temp = x_limit.l0;
		x_limit.l0 = x_limit.l1;
		x_limit.l1 = temp;
	}

    // Handle thickness
    uint8_t y_end  = y + thickness - 1;
    if (y_end >= SSD1306_H) y_end = SSD1306_H_LIMIT;
	if (y_end < y) return;  // Skip if thickness is 0 or overflowed

    // Draw thick line
    for (uint8_t y_pos = y; y_pos <= y_end ; y_pos++) {
        M_Page_Mask mask = page_masks[y_pos];

        for (uint8_t x_pos = x_limit.l0; x_pos <= x_limit.l1; x_pos++) {
            frame_buffer[mask.page][x_pos] |= mask.bitmask;
        }
    }
}

//! compute vertical line
void prefill_verLine(
	uint8_t x, Limit y_limit, uint8_t thickness, uint8_t mirror
) {
    // Validate coordinates
    if (x >= SSD1306_W) return;

	// Clamp to display bounds
    if (y_limit.l0 >= SSD1306_H) y_limit.l0 = SSD1306_H_LIMIT;
    if (y_limit.l1 >= SSD1306_H) y_limit.l1 = SSD1306_H_LIMIT;

	// Handle mirroring
	if (mirror) {
		y_limit.l0 = SSD1306_H_LIMIT - y_limit.l0;
		y_limit.l1 = SSD1306_H_LIMIT - y_limit.l1;
	}

	// Ensure y1 <= y2 (swap if needed)
	if (y_limit.l0 > y_limit.l1) {
		uint8_t temp = y_limit.l0;
		y_limit.l0 = y_limit.l1;
		y_limit.l1 = temp;
	}

    // Handle thickness
    uint8_t x_end = x + thickness - 1;
    if (x_end >= SSD1306_W) x_end = SSD1306_W_LIMIT;
	if (x_end < x) return;  // Skip if thickness causes overflow

    // // Draw vertical line with thickness
    // for (uint8_t y_pos = y_limit.l0; y_pos <= y_limit.l1; y_pos++) {
    //     M_Page_Mask mask = page_masks[y_pos];
		
    //     for (uint8_t x_pos = x; x_pos <= x_end; x_pos++) {
    //         frame_buffer[mask.page][x_pos] |= mask.bitmask;
    //     }
    // }

	//# Optimized: save 500-700 us
	uint8_t x_len = x_end - x + 1;  // Precompute length

	for (uint8_t y_pos = y_limit.l0; y_pos <= y_limit.l1; y_pos++) {
		M_Page_Mask mask = page_masks[y_pos];
		uint8_t* row_start = &frame_buffer[mask.page][x];  	// Get row pointer

		for (uint8_t i = 0; i < x_len; i++) {
			row_start[i] |= mask.bitmask;  					// Sequential access
		}
	}
}

//! compute line (Bresenham's algorithm)
void prefill_line(M_Point p0, M_Point p1, uint8_t thickness) {
    // Clamp coordinates to display bounds
    p0.x = (p0.x < SSD1306_W) ? p0.x : SSD1306_W_LIMIT;
    p0.y = (p0.y < SSD1306_H) ? p0.y : SSD1306_H_LIMIT;
    p1.x = (p1.x < SSD1306_W) ? p1.x : SSD1306_W_LIMIT;
    p1.y = (p1.y < SSD1306_H) ? p1.y : SSD1306_H_LIMIT;

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
			if (p0.x < SSD1306_W && p0.y < SSD1306_H) {
                M_Page_Mask mask = page_masks[p0.y];
                frame_buffer[mask.page][p0.x] |= mask.bitmask;
			}
		} else {
			uint8_t x_end = p0.x + radius;
			uint8_t y_end = p0.y + radius;

			// Calculate bounds (branchless min/max)
			uint8_t x_start = p0.x > radius ? p0.x - radius : 0;
			uint8_t y_start = p0.y > radius ? p0.y - radius : 0;

			if (x_end > SSD1306_W_LIMIT) x_end = SSD1306_W_LIMIT;
			if (y_end > SSD1306_H_LIMIT) y_end = SSD1306_H_LIMIT;
			uint8_t width = x_end - x_start + 1;

			// Optimized row filling
			for (uint8_t y = y_start; y <= y_end; y++) {
				M_Page_Mask mask = page_masks[y];
				uint8_t *row = fb_base + (mask.page * SSD1306_W) + x_start;
				
				// Optimized filling based on width
				if (width <= 4) {
					// Fully unrolled for common cases
					if (width > 0) row[0] |= mask.bitmask;
					if (width > 1) row[1] |= mask.bitmask;
					if (width > 2) row[2] |= mask.bitmask;
					if (width > 3) row[3] |= mask.bitmask;
				} else {
					// For larger widths, use memset-style optimization
					uint8_t pattern = mask.bitmask;
					for (uint8_t i = 0; i < width; i++) {
						row[i] |= pattern;
					}
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

//! compute lines
void prefill_lines(M_Point *pts, uint8_t num_pts, uint8_t thickness) {
    // Early exit if not enough points (need at least 2 points for a line)
    if (num_pts < 2) return;
    
    // Draw connected lines between all points
    for (uint8_t i = 0; i < num_pts - 1; i++) {
        prefill_line(pts[i], pts[i+1], thickness);
    }
}

// #include "lib_rand.h"

uint8_t myvalues[16] = { 30, 50, 60, 40, 20, 50, 30, 10, 35, 10, 20, 30, 40, 50, 60, 20 };
 

void test_lines() {
	// seed(0x12345678);
	int y = 0;
	
    //! hor-ver lines
	for(int8_t i = 0; i<sizeof(myvalues); i++) {
		// uint8_t rand_byte_low = rand() & 0xFF;
		Limit limit = { l0: 0, l1: myvalues[i] };
		// prefill_horLine(y, limit, 3, 0);
		// prefill_horLine(y, 0, myvalues[i], 3, 1);
		prefill_verLine(y, limit, 3, 0);
		// prefill_verLine(y, 0, myvalues[i], 3, 1);
		y += 4;
	}

    //! line
    for(uint8_t x=0;x<SSD1306_W;x+=16) {
		M_Point point_a0 = { x: x, y: 0 };
		M_Point point_a1 = { x: SSD1306_W, y: y };
		M_Point point_b0 = { x: SSD1306_W-x, y: SSD1306_H };
		M_Point point_b1 = { x: 0, y: SSD1306_H-y };
		
		prefill_line(point_a0, point_a1, 1);
		prefill_line(point_b0, point_b1, 1);

		y+= SSD1306_H/8;
	}
}