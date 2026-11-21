#include "ch32fun.h"

#include "../../../Mess-libs/spi/spi_port.h"
#include "../../../Mess-libs/spi/fun_st77352.h"

#define TFT_CS    PA1
#define TFT_DC    PD4
#define TFT_RESET PA2

static uint16_t st7735_colors[] = {
	ST_RED, ST_GREEN, ST_BLUE, ST_YELLOW, ST_CYAN, ST_MAGENTA, ST_ORANGE
};

static void st7735_test(void) {
	st7735_fill(ST_PURPLE);
	tft_print("Hello World 222", 0, 0, ST_WHITE, ST_PURPLE);

	static uint8_t color_idx = 0;
	uint16_t color = st7735_colors[color_idx++ % (sizeof(st7735_colors) / sizeof(uint16_t))];

	//# draw vertical lines
	static uint8_t idx = 0;
	uint8_t x_value = idx++ % ST7735_WIDTH;
	tft_draw_line(x_value, 0, x_value, ST7735_HEIGHT, color, 1);

	//# draw horizontal lines
	uint8_t y_value = idx++ % ST7735_HEIGHT;
	tft_draw_line(0, y_value, ST7735_WIDTH, y_value, color, 1);

	//# draw diagonal lines
	tft_draw_line(x_value, y_value, x_value + 30, y_value + 80, color, 1);

	//# draw rectangles
	tft_draw_rect(x_value, y_value, 20, 20, color);

	//# draw random rectangles
	tft_fill_rect(x_value + 20, y_value + 20, 20, 20, color);

	//# draw poly
	int16_t triangle_x[] = {10, 40, 80};
	int16_t triangle_y[] = {20, 60, 70};

	tft_draw_poly(triangle_x, triangle_y, 3, ST_RED, 3);

	int16_t square_x[] = {10, 60, 60, 10};
	int16_t square_y[] = {10, 10, 60, 60};
	tft_draw_poly(square_x, square_y, 4, ST_ORANGE, 3);

	Point16_t triangle[] = {{30, 30}, {40, 60}, {80, 70}};
	tft_draw_poly2(triangle, 3, ST_YELLOW, 3);

	tft_draw_solid_poly2(triangle, 3, ST_RED, ST_WHITE, 2);

	Point16_t square[] = {{10, 10}, {60, 10}, {60, 60}, {10, 60}};
	tft_draw_poly2(square, 4, ST_GREEN, 3);

	tft_draw_circle((Point16_t){ 80, 50 }, 20, 0x07E0); // Green circle with radius = 30
	tft_draw_circle((Point16_t){ 70, 30 }, 30, 0x001F); // Blue circle with radius = 40

	tft_draw_filled_circle((Point16_t){ 90, 50 }, 10, 0x07E0);
	tft_draw_ring((Point16_t){ 100, 50 }, 20, 0x07E0, 5); // Green ring with radius = 30 and width = 5
}

int main(void) {
	SystemInit();
	funGpioInitAll();

	SPI_INIT(TFT_RESET, TFT_DC);
	st7735_init(160, 80, TFT_CS);
	st7735_test();

	while (1) {

	}
}
