#include "modSSD1306.h"
#include "ssd1306_poligon.h"

//! compute circle (Bresenham's algorithm)
void prefill_circle(
	M_Point p0, uint8_t radius, uint8_t fill
) {
	// Validate center coordinates
	if (p0.x >= SSD1306_W || p0.y >= SSD1306_H) return;

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
            prefill_fastHorLine(y_top, x_start, x_end);     // Top half
            prefill_fastHorLine(y_bottom, x_start, x_end);  // Bottom half
		} else {
			uint8_t xy_start 	= p0.x + y;
			uint8_t xy_end   	= p0.x - y;
			uint8_t yx_start 	= p0.y + x;
			uint8_t yx_end   	= p0.y - x;

			// Draw all 8 symmetric points (using precomputed page_masks)
			prefill_pixel(x_end		, y_bottom); 	// Octant 1
			prefill_pixel(x_start	, y_bottom); 	// Octant 2
			prefill_pixel(x_start	, y_top); 		// Octant 3
			prefill_pixel(x_end		, y_top); 		// Octant 4
			prefill_pixel(xy_end	, yx_start); 	// Octant 5
			prefill_pixel(xy_start	, yx_start); 	// Octant 6
			prefill_pixel(xy_start	, yx_end); 		// Octant 7
			prefill_pixel(xy_end	, yx_end); 		// Octant 8
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

//! compute pie
void prefill_pie(M_Point center, uint8_t radius, int16_t start_angle, int16_t end_angle) {    
    // Draw center point
    prefill_pixel(center.x, center.y);
    
    // Special case: full circle
    if (start_angle == end_angle) {
        prefill_circle(center, radius, 1); // Fill entire circle
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
		prefill_line(p0, p1, 1);
        
        // Break conditions
        if (angle == end_angle) break;
        
        // Move to next angle
        angle = (angle + step) % 360;
        
        // Handle wrap-around
        if (start_angle > end_angle && angle < start_angle && angle > end_angle) break;
    }
}


//! compute ring (Bresenham's algorithm)
void prefill_ring(
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

            // Skip pixels outside valid screen bounds
            if (y_top >= 0 && y_top < SSD1306_H) {
                M_Page_Mask mask_t = page_masks[y_top];
                uint8_t* row_t = frame_base + (mask_t.page * SSD1306_W);

                if (x_left >= 0 && x_left < SSD1306_W) 		row_t[x_left]  |= mask_t.bitmask;
                if (x_start >= 0 && x_start < SSD1306_W) 	row_t[x_start] |= mask_t.bitmask;
                if (x_end >= 0 && x_end < SSD1306_W) 		row_t[x_end]   |= mask_t.bitmask;
                if (x_right >= 0 && x_right < SSD1306_W) 	row_t[x_right] |= mask_t.bitmask;
            }

            if (y_bottom >= 0 && y_bottom < SSD1306_H) {
                M_Page_Mask mask_b = page_masks[y_bottom];
                uint8_t* row_b = frame_base + (mask_b.page * SSD1306_W);

                if (x_left >= 0 && x_left < SSD1306_W) 		row_b[x_left]  |= mask_b.bitmask;
                if (x_start >= 0 && x_start < SSD1306_W) 	row_b[x_start] |= mask_b.bitmask;
                if (x_end >= 0 && x_end < SSD1306_W) 		row_b[x_end]   |= mask_b.bitmask;
                if (x_right >= 0 && x_right < SSD1306_W) 	row_b[x_right] |= mask_b.bitmask;
            }

            // Optimized Bresenham step
            int16_t e2 = err;
            if (e2 <= y) err += ++y * 2 + 1;  // y*2+1
            if (e2 > x)  err += ++x * 2 + 1;  // x*2+1
        } while (x <= 0);
    }
}


void test_circles() {
    int y = 0;

    for (int8_t i = 0; i<4; i++) {
		uint8_t should_fill = i > 1 ? 1 : 0;
		prefill_circle((M_Point){ 110, y }, 5, should_fill);

        if (i > 1) {
            prefill_ring((M_Point){ 90, y + 12 }, 7, 2, 0, 180);
        }
        
		// compute_pie(point, 20, 0, 100);
		y += 14;
	}
}