#include "ssd1306_line.h"

//! compute poligon
void prefill_poly(M_Point *pts, uint8_t num_pts, uint8_t thickness) {
    if (num_pts < 3) return;  // Need at least 3 points for a polygon
    prefill_lines(pts, num_pts, thickness);
    prefill_line(pts[num_pts-1], pts[0], thickness);
}

//! optimized: compute filled polygon
void prefill_solid_poly(M_Point *pts, uint8_t num_pts) {
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
            if (x1 < x2) prefill_fastHorLine(y, x1, x2);
        }
    }
}

//! compute rectangle
void prefill_rect(
	M_Point p0, Area area, uint8_t fill, uint8_t mirror
) {
	// Validate coordinates
	if (p0.x >= SSD1306_W || p0.y >= SSD1306_H) return;

	// Clamp to display bounds
	uint16_t x_end = p0.x + area.w;
	uint16_t y_end = p0.y + area.h;
	if (x_end >= SSD1306_W) area.w = SSD1306_W_LIMIT - p0.x;
	if (y_end >= SSD1306_H) area.h = SSD1306_H_LIMIT - p0.y;

	// Handle mirroring
	if (mirror) {
		p0.x = SSD1306_W_LIMIT - p0.x;
		area.w = SSD1306_W_LIMIT - area.w;
	}

	// Draw rectangle with optional fill
	Limit hLimit = { l0: p0.x, l1: x_end };
    if (fill) {
		// Filled rectangle using horizontal lines (faster for row-major displays)
		for (uint8_t y_pos = p0.y; y_pos <= y_end; y_pos++) {
			prefill_horLine(y_pos, hLimit, 1, 0);
		}
    } else {
		Limit vLimit = { l0: p0.y + 1, l1: y_end - 1 };

        // Outline only
        prefill_horLine(p0.y, hLimit, 1, 0);     	// Top edge
        prefill_horLine(y_end, hLimit, 1, 0);    	// Bottom edge
        prefill_verLine(p0.x, vLimit, 1, 0); 		// Left edge
        prefill_verLine(x_end, vLimit, 1, 0); 		// Right edge
    }
}


void test_polys() {
    int y = 0;

    //! rectangles
    for (int8_t i = 0; i<4; i++) {
		uint8_t should_fill = i > 1 ? 1 : 0;
		prefill_rect((M_Point){ 84, y }, (Area) { 15, 5 }, should_fill, 0);
		y += 7;
	}

    //! zigzag
    M_Point zigzag[] = {
        (M_Point) { 60, 8 },
        (M_Point) { 50, 15 },
        (M_Point) { 80, 8 },
        (M_Point) { 70, 0 },
        (M_Point) { 70, 20 }
    };

    uint8_t pt_count = sizeof(zigzag)/sizeof(M_Point);
    prefill_solid_poly(zigzag, pt_count);

    M_Point zigzag2[4];
    memcpy(zigzag2, zigzag, sizeof(zigzag));  // Fast copy

    for (int i = 0; i < sizeof(zigzag)/sizeof(M_Point); i++) {
        zigzag2[i].y += 24;  // Add 20 to each x-coordinate
    }
    prefill_poly(zigzag2, pt_count, 1);


    // Concave polygon: Star (22px tall)
    M_Point star[] = {
        {12 , 0},  // Top point
        {16 , 8}, // Right upper
        {24 , 8}, // Right outer
        {18 , 14}, // Right inner
        {22 , 22}, // Bottom right
        {12 , 16}, // Bottom center
        {2  , 22},  // Bottom left
        {6  , 14},  // Left inner
        {0  , 8},  // Left outer
        {8  , 8}   // Left upper
    };

    // Convex polygon: Quad (12px tall)
    static M_Point quad[] = {
        {6  , 24},  // Bottom-left (aligned with star's left)
        {18 , 24}, // Bottom-right (centered)
        {22 , 34}, // Top-right (matches star width - unchanged)
        {2  , 34}   // Top-left (aligned - unchanged)
    };

    // Self-intersecting: Hourglass (12px tall)
    static M_Point hourglass[] = {
        {6  , 38},   // Top-left (aligned with quad's bottom-left)
        {18 , 38},  // Top-right (aligned with quad's bottom-right)
        {6  , 52},   // Bottom-left
        {18 , 52}   // Bottom-right
    };

    //! star
    pt_count = sizeof(star)/sizeof(M_Point);
    prefill_solid_poly(star, pt_count);

    // Shift star right by 25px
    for (int i = 0; i < sizeof(star)/sizeof(M_Point); i++) {
        star[i].x += 25;  // Add 20 to each x-coordinate
    }
    prefill_poly(star, pt_count, 1);

    //! quad
    pt_count = sizeof(quad)/sizeof(M_Point);
    prefill_solid_poly(quad, pt_count);

    // Shift quad right by 25px
    M_Point quad2[4];
    memcpy(quad2, quad, sizeof(quad));  // Fast copy

    for (int i = 0; i < sizeof(quad)/sizeof(M_Point); i++) {
        quad2[i].x += 25;  // Add 20 to each x-coordinate
    }
    prefill_poly(quad2, pt_count, 1);

    //! hourglass
    pt_count = sizeof(hourglass)/sizeof(M_Point);
    prefill_solid_poly(hourglass, pt_count);

    // Shift hourglass right by 25px
    M_Point hourglass2[4];
    memcpy(hourglass2, hourglass, sizeof(hourglass));  // Fast copy

    for (int i = 0; i < sizeof(hourglass)/sizeof(M_Point); i++) {
        hourglass2[i].x += 25;  // Add 20 to each x-coordinate
    }
    prefill_poly(hourglass2, pt_count, 1);
}