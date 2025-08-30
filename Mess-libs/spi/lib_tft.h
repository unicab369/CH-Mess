// stolen and adjusted from: GitHub: https://github.com/limingjie/

#include "ch32fun.h"
#include "font5x7.h"

#define ST7735_W    160

#ifndef TFT_X_OFFSET
    #define TFT_X_OFFSET 1
#endif

#ifndef TFT_Y_OFFSET
    #define TFT_Y_OFFSET 26
#endif

// 5x7 Font
#define FONT_WIDTH  5  // Font width
#define FONT_HEIGHT 7  // Font height

#define RGB565(r, g, b) ((((r)&0xF8) << 8) | (((g)&0xFC) << 3) | ((b) >> 3))
#define BGR565(r, g, b) ((((b)&0xF8) << 8) | (((g)&0xFC) << 3) | ((r) >> 3))
#define RGB         RGB565

#define BLACK       RGB(0, 0, 0)
#define NAVY        RGB(0, 0, 123)
#define DARKGREEN   RGB(0, 125, 0)
#define DARKCYAN    RGB(0, 125, 123)
#define MAROON      RGB(123, 0, 0)
#define PURPLE      RGB(123, 0, 123)
#define OLIVE       RGB(123, 125, 0)
#define LIGHTGREY   RGB(198, 195, 198)
#define DARKGREY    RGB(123, 125, 123)
#define BLUE        RGB(0, 0, 255)
#define GREEN       RGB(0, 255, 0)
#define CYAN        RGB(0, 255, 255)
#define RED         RGB(255, 0, 0)
#define MAGENTA     RGB(255, 0, 255)
#define YELLOW      RGB(255, 255, 0)
#define WHITE       RGB(255, 255, 255)
#define ORANGE      RGB(255, 165, 0)
#define GREENYELLOW RGB(173, 255, 41)
#define PINK        RGB(255, 130, 198)

static uint16_t colors[] = {
    BLACK, NAVY, DARKGREEN, DARKCYAN, MAROON, PURPLE, OLIVE,  LIGHTGREY,   DARKGREY, BLUE,
    GREEN, CYAN, RED,       MAGENTA,  YELLOW, WHITE,  ORANGE, GREENYELLOW, PINK,
};


// interfaces: use these to control CS pin
#ifndef INTF_TFT_START_WRITE
    void INTF_TFT_START_WRITE() {}
#endif

#ifndef INTF_TFT_END_WRITE
    void INTF_TFT_END_WRITE() {}
#endif

void INTF_TFT_SET_WINDOW(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void INTF_TFT_SEND_BUFF(const uint8_t* buffer, uint16_t size, uint16_t repeat);
void INTF_TFT_SEND_COLOR(uint16_t color);

static uint8_t  _frame_buffer[ST7735_W << 1] = {0};
static uint16_t _cursor_x                  = 0;
static uint16_t _cursor_y                  = 0;      // Cursor position (x, y)

static uint8_t  _buffer[ST7735_W << 1] = {0};    // DMA buffer, long enough to fill a row.

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

    START_WRITE();
    INTF_TFT_SET_WINDOW(_cursor_x, _cursor_y, _cursor_x + width - 1, _cursor_y + height - 1);
    INTF_TFT_SEND_BUFF(_frame_buffer, sz, 1);
    END_WRITE();
}

void tft_print(const char* str) {
    uint8_t font_width = 5; // Assuming a fixed width for the font

    while (*str) {
        tft_print_char(*str++, 7, font_width, 0xFFFF, 0x0000); // 7x5 font size
        _cursor_x += font_width + 1;
    }
}


void tft_set_cursor(uint16_t x, uint16_t y) {
    _cursor_x = x + TFT_X_OFFSET;
    _cursor_y = y + TFT_Y_OFFSET;
}

void tft_fill_rect(
    uint16_t x, uint16_t y,
    uint16_t width, uint16_t height, uint16_t color
) {
    x += TFT_X_OFFSET;
    y += TFT_Y_OFFSET;

    uint16_t sz = 0;
    for (uint16_t x = 0; x < width; x++) {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    INTF_TFT_SET_WINDOW(x, y, x + width - 1, y + height - 1);
    INTF_TFT_SEND_BUFF(_buffer, sz, height);
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
    x += TFT_X_OFFSET;
    y += TFT_Y_OFFSET;

    START_WRITE();
    INTF_TFT_SET_WINDOW(x, y, x + width - 1, y + height - 1);
    INTF_TFT_SEND_BUFF(bitmap, width * height << 1, 1);
    END_WRITE();
}


// stolen and adjusted from: GitHub: https://github.com/limingjie/

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
    x += TFT_X_OFFSET;
    y += TFT_Y_OFFSET;
    START_WRITE();
    INTF_TFT_SET_WINDOW(x, y, x, y);
    INTF_TFT_SEND_COLOR(color);
    END_WRITE();
}

//! private
static void _draw_fast_vLine(
    int16_t x, int16_t y, int16_t h, uint16_t color
) {
    x += TFT_X_OFFSET;
    y += TFT_Y_OFFSET;

    uint16_t sz = 0;
    for (int16_t j = 0; j < h; j++) {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    INTF_TFT_SET_WINDOW(x, y, x, y + h - 1);
    INTF_TFT_SEND_BUFF(_buffer, sz, 1);
    END_WRITE();
}


//! private
static void _draw_fast_hLine(
    int16_t x, int16_t y, int16_t w, uint16_t color
) {
    x += TFT_X_OFFSET;
    y += TFT_Y_OFFSET;

    uint16_t sz = 0;
    for (int16_t j = 0; j < w; j++) {
        _buffer[sz++] = color >> 8;
        _buffer[sz++] = color;
    }

    START_WRITE();
    INTF_TFT_SET_WINDOW(x, y, x + w - 1, y);
    INTF_TFT_SEND_BUFF(_buffer, sz, 1);
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


