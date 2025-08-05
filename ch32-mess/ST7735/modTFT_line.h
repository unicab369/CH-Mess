// stolen and adjusted from: GitHub: https://github.com/limingjie/

#include "modST7735.h"

// Draw line helpers
#define _diff(a, b) ((a > b) ? (a - b) : (b - a))
#define _swap_int16(a, b)   {                           \
                                int16_t temp = a;       \
                                a            = b;       \
                                b            = temp;    \
                            }

//! draw pixel
void tft_draw_pixel(
    uint16_t x, uint16_t y, uint16_t color
) {
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;
    START_WRITE();
    tft_set_window(x, y, x, y);
    tft_send_color(color);
    END_WRITE();
}

//! private
static void _draw_fast_vLine(
    int16_t x, int16_t y, int16_t h, uint16_t color
) {
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    uint16_t sz = 0;
    for (int16_t j = 0; j < h; j++) {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    tft_set_window(x, y, x, y + h - 1);
    tft_send_DMA(_buffer, sz, 1);
    END_WRITE();
}


//! private
static void _draw_fast_hLine(
    int16_t x, int16_t y, int16_t w, uint16_t color
) {
    x += ST7735_X_OFFSET;
    y += ST7735_Y_OFFSET;

    uint16_t sz = 0;
    for (int16_t j = 0; j < w; j++) {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    tft_set_window(x, y, x + w - 1, y);
    tft_send_DMA(_buffer, sz, 1);
    END_WRITE();
}

//! draw_line_bresenham
static void _draw_line_bresenham(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1, uint16_t color, uint8_t width
) {
    uint8_t steep = _diff(y1, y0) > _diff(x1, x0);
    if (steep) {
        _swap_int16(x0, y0);
        _swap_int16(x1, y1);
    }

    if (x0 > x1) {
        _swap_int16(x0, x1);
        _swap_int16(y0, y1);
    }

    int16_t dx   = x1 - x0;
    int16_t dy   = _diff(y1, y0);
    int16_t err  = dx >> 1;
    int16_t step = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        for (int16_t w = -(width / 2); w <= width / 2; w++) {
            if (steep) {
                tft_draw_pixel(y0 + w, x0, color); // Draw perpendicular pixels for width
            } else {
                tft_draw_pixel(x0, y0 + w, color); // Draw perpendicular pixels for width
            }
        }
        err -= dy;
        if (err < 0) {
            err += dx;
            y0 += step;
        }
    }
}


//! draw line
void tft_draw_line(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1, uint16_t color, uint8_t width
) {
    if (x0 == x1) {
        if (y0 > y1) _swap_int16(y0, y1);
        _draw_fast_vLine(x0, y0, y1 - y0 + 1, color);
    }
    else if (y0 == y1) {
        if (x0 > x1) _swap_int16(x0, x1);
        _draw_fast_hLine(x0, y0, x1 - x0 + 1, color);
    }
    else {
        _draw_line_bresenham(x0, y0, x1, y1, color, width);
    }
}


//! draw rectangle
void tft_draw_rect(
    uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color
) {
    _draw_fast_hLine(x, y, width, color);
    _draw_fast_hLine(x, y + height - 1, width, color);
    _draw_fast_vLine(x, y, height, color);
    _draw_fast_vLine(x + width - 1, y, height, color);
}


typedef struct {
    int16_t x; // X-coordinate
    int16_t y; // Y-coordinate
} Point16_t;

//! draw polygon
static void _draw_poly(
    const int16_t* vertices_x, // Array of x-coordinates of vertices
    const int16_t* vertices_y, // Array of y-coordinates of vertices
    uint16_t num_vertices,     // Number of vertices in the polygon
    uint16_t color,            // Color of the polygon edges
    uint8_t width              // Width of the edges
) {
    if (num_vertices < 3) return; // A polygon must have at least 3 vertices

    for (uint16_t i = 0; i < num_vertices; i++) {
        int16_t x0 = vertices_x[i];
        int16_t y0 = vertices_y[i];
        int16_t x1 = vertices_x[(i + 1) % num_vertices]; // Wrap around to connect last vertex to first
        int16_t y1 = vertices_y[(i + 1) % num_vertices];

        tft_draw_line(x0, y0, x1, y1, color, width); // Draw edge with specified width
    }
}

//! draw polygon
static void tft_draw_poly2(
    const Point16_t* vertices, uint16_t num_vertices,
    uint16_t color, uint8_t width
) {
    if (num_vertices < 3) return; // A polygon must have at least 3 vertices

    for (uint16_t i = 0; i < num_vertices; i++) {
        Point16_t p0 = vertices[i];
        Point16_t p1 = vertices[(i + 1) % num_vertices]; // Wrap around to connect last vertex to first

        tft_draw_line(p0.x, p0.y, p1.x, p1.y, color, width); // Draw edge with specified width
    }
}

//! draw solid polygon
static void tft_draw_solid_poly(
    const Point16_t* vertices, uint16_t num_vertices,
    uint16_t fill_color, uint16_t edge_color, uint8_t edge_width
) {
    if (num_vertices < 3) return;  // A polygon must have at least 3 vertices

    // Find the bounding box of the polygon
    int16_t min_y = vertices[0].y, max_y = vertices[0].y;
    
    for (uint16_t i = 1; i < num_vertices; i++) {
        if (vertices[i].y < min_y) min_y = vertices[i].y;
        if (vertices[i].y > max_y) max_y = vertices[i].y;
    }

    // Scan through each row of the polygon
    for (int16_t y = min_y; y <= max_y; y++) {
        // Find all intersections with the polygon edges
        int16_t intersections[20]; // Adjust size as needed
        uint8_t num_intersections = 0;

        for (uint16_t i = 0; i < num_vertices; i++) {
            Point16_t p0 = vertices[i];
            Point16_t p1 = vertices[(i + 1) % num_vertices];

            // Skip horizontal edges
            if (p0.y == p1.y) continue;

            // Check if y is between the edge's y range
            if ((y >= p0.y && y < p1.y) || (y >= p1.y && y < p0.y)) {
                // Calculate intersection x coordinate
                int16_t x = p0.x + ((int32_t)(y - p0.y) * (p1.x - p0.x)) / (p1.y - p0.y);
                intersections[num_intersections++] = x;
            }
        }

        // OPTIMIZATION #1: Replace bubble sort with insertion sort
        for (uint8_t i = 1; i < num_intersections; i++) {
            int16_t key = intersections[i];
            int8_t j = i - 1;
            while (j >= 0 && intersections[j] > key) {
                intersections[j + 1] = intersections[j];
                j--;
            }
            intersections[j + 1] = key;
        }

        // Fill between pairs of intersections
        for (uint8_t i = 0; i < num_intersections; i += 2) {
            if (i + 1 >= num_intersections) break;
            int16_t x0 = intersections[i];
            int16_t x1 = intersections[i + 1];
            
            // Draw horizontal line between intersections
            tft_draw_line(x0, y, x1, y, fill_color, 1);
        }
    }
    
    // Optionally draw the edges
    if (edge_width > 0) {
        tft_draw_poly2(vertices, num_vertices, edge_color, edge_width); // Draw edges with specified width
    }
}


//! optimized draw solid polygon
static void tft_draw_solid_poly2(
    const Point16_t* vertices, uint16_t num_vertices,
    uint16_t fill_color, uint16_t edge_color, uint8_t edge_width
) {
    if (num_vertices < 3) return;  // A polygon must have at least 3 vertices

    // Find the bounding box of the polygon
    int16_t min_y = vertices[0].y, max_y = vertices[0].y;
    
    for (uint16_t i = 1; i < num_vertices; i++) {
        if (vertices[i].y < min_y) min_y = vertices[i].y;
        if (vertices[i].y > max_y) max_y = vertices[i].y;
    }

    // OPTIMIZATION #2: Precompute edge information
    typedef struct {
        int16_t y_min, y_max;
        int32_t x_step;  // Fixed-point slope (dx/dy)
        int32_t x_curr;  // Fixed-point current x
    } EdgeInfo;
    
    EdgeInfo edges[num_vertices];
    uint8_t valid_edges = 0;
    
    for (uint16_t i = 0; i < num_vertices; i++) {
        const Point16_t* p0 = &vertices[i];
        const Point16_t* p1 = &vertices[(i + 1) % num_vertices];
        
        if (p0->y == p1->y) continue; // Skip horizontal edges
        
        // Order vertices top to bottom
        int16_t y_min, y_max, x_start;
        if (p0->y < p1->y) {
            y_min = p0->y;
            y_max = p1->y;
            x_start = p0->x;
            edges[valid_edges].x_curr = x_start << 16; // Fixed-point init
            edges[valid_edges].x_step = ((int32_t)(p1->x - p0->x) << 16) / (p1->y - p0->y);
        } else {
            y_min = p1->y;
            y_max = p0->y;
            x_start = p1->x;
            edges[valid_edges].x_curr = x_start << 16;
            edges[valid_edges].x_step = ((int32_t)(p0->x - p1->x) << 16) / (p0->y - p1->y);
        }
        
        edges[valid_edges].y_min = y_min;
        edges[valid_edges].y_max = y_max;
        valid_edges++;
    }

    // Scan through each row of the polygon
    for (int16_t y = min_y; y <= max_y; y++) {
        int16_t intersections[20];
        uint8_t num_intersections = 0;

        // Find active edges
        for (uint8_t i = 0; i < valid_edges; i++) {
            if (y >= edges[i].y_min && y < edges[i].y_max) {
                // Calculate x intersection (with rounding)
                intersections[num_intersections++] = (edges[i].x_curr + (1 << 15)) >> 16;
                // Update x for next scanline
                edges[i].x_curr += edges[i].x_step;
            }
        }

        // Insertion sort (from previous optimization)
        for (uint8_t i = 1; i < num_intersections; i++) {
            int16_t key = intersections[i];
            int8_t j = i - 1;
            while (j >= 0 && intersections[j] > key) {
                intersections[j + 1] = intersections[j];
                j--;
            }
            intersections[j + 1] = key;
        }

        // Fill between pairs
        for (uint8_t i = 0; i < num_intersections; i += 2) {
            if (i + 1 >= num_intersections) break;
            int16_t x0 = intersections[i];
            int16_t x1 = intersections[i + 1];
            if (x1 > x0) {
                tft_draw_line(x0, y, x1, y, fill_color, 1);
            }
        }
    }
    
    // Optionally draw the edges
    if (edge_width > 0) {
        tft_draw_poly2(vertices, num_vertices, edge_color, edge_width); // Draw edges with specified width
    }
}

//! draw circle
static void tft_draw_circle(
    Point16_t center, int16_t radius, uint16_t color
) {
    int16_t x = 0;
    int16_t y = radius;
    int16_t err = 1 - radius; // Initial error term

    while (x <= y) {
        // Draw symmetric points in all octants
        tft_draw_pixel(center.x + x, center.y + y, color); // Octant 1
        tft_draw_pixel(center.x - x, center.y + y, color); // Octant 2
        tft_draw_pixel(center.x + x, center.y - y, color); // Octant 3
        tft_draw_pixel(center.x - x, center.y - y, color); // Octant 4
        tft_draw_pixel(center.x + y, center.y + x, color); // Octant 5
        tft_draw_pixel(center.x - y, center.y + x, color); // Octant 6
        tft_draw_pixel(center.x + y, center.y - x, color); // Octant 7
        tft_draw_pixel(center.x - y, center.y - x, color); // Octant 8

        if (err < 0) {
            err += 2 * x + 3;
        } else {
            err += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}


//! draw filled circle
static void tft_draw_filled_circle(
    Point16_t p0, int16_t radius, uint16_t color
) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    //# optimize for radius <= 4
    if (radius <= 4) {
        _draw_fast_hLine(p0.x - radius, p0.y, radius + radius + 1, color);
        for (int16_t i = 1; i <= radius; i++) {
            int16_t w = (radius - i) + (radius - i) + 1;
            _draw_fast_hLine(p0.x - (radius - i), p0.y + i, w, color);
            _draw_fast_hLine(p0.x - (radius - i), p0.y - i, w, color);
        }
        return;
    }

    while (x >= y) {
        // Draw horizontal spans for each octant
        _draw_fast_hLine(p0.x - x, p0.y + y, 2 * x + 1, color);  // Bottom span
        _draw_fast_hLine(p0.x - x, p0.y - y, 2 * x + 1, color);  // Top span
        _draw_fast_hLine(p0.x - y, p0.y + x, 2 * y + 1, color);  // Right span
        _draw_fast_hLine(p0.x - y, p0.y - x, 2 * y + 1, color);  // Left span

        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
}

//! draw ring
static void tft_draw_ring(
    Point16_t center, int16_t radius, uint16_t color, uint8_t width
) {
    tft_draw_filled_circle(center, radius, color); // Draw outer circle
    tft_draw_filled_circle(center, radius - width, PURPLE); // Draw inner circle
}


void tft_line_tests() {
    //! dots test
    tft_draw_pixel(rand8() % 160, rand8() % 80, colors[rand8() % 19]);

    // //! draw vertical lines
    static uint8_t x_idx = 0;
    tft_draw_line(x_idx, 0, x_idx, 80, colors[rand8() % 19], 1);
    x_idx += 1;
    if (x_idx >= 160) x_idx = 0;

    // //! draw horizontal lines
    static uint8_t y_idx = 0;
    tft_draw_line(0, y_idx, 180, y_idx, colors[rand8() % 19], 1);
    y_idx += 1;
    if (y_idx >= 80) y_idx = 0;

    //! draw random lines
    tft_draw_line(0, 0, 70, 70, RED, 5);

    tft_draw_line(rand8() % 160, rand8() % 80, rand8() % 160, rand8() % 80, colors[rand8() % 19], 1);

    //! draw poly
    int16_t triangle_x[] = {10, 40, 80};
    int16_t triangle_y[] = {20, 60, 70};

    // _draw_poly(triangle_x, triangle_y, 3, RED, 3);

    // int16_t square_x[] = {10, 60, 60, 10};
    // int16_t square_y[] = {10, 10, 60, 60};
    // _draw_poly(square_x, square_y, 4, RED, 3);

    Point16_t triangle[] = {{10, 20}, {40, 60}, {80, 70}};
    // tft_draw_poly2(triangle, 3, RED, 3);

    tft_draw_solid_poly2(triangle, 3, RED, WHITE, 2);

    // Point16_t square[] = {{10, 10}, {60, 10}, {60, 60}, {10, 60}};
    // _draw_poly2(square, 4, RED, 3);

    // tft_draw_circle((Point16_t){ 50, 50 }, 20, 0x07E0); // Green circle with radius = 30
    // tft_draw_circle((Point16_t){ 30, 30 }, 30, 0x001F); // Blue circle with radius = 40

    // tft_draw_filled_circle((Point16_t){ 50, 50 }, 10, 0x07E0);
    // tft_draw_ring((Point16_t){ 50, 50 }, 20, 0x07E0, 5); // Green ring with radius = 30 and width = 5
}