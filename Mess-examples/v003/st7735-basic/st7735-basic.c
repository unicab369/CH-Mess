#include "ch32fun.h"

/* This library's SPI backend uses SPI1: PC5=SCK and PC6=MOSI. */
#include "../../../Mess-libs/spi/lib/lib_spi.h"
#include "../../../Mess-libs/spi/fun_st77352.h"

#define TFT_CS   PC4
#define TFT_DC   PC0
#define TFT_RESET PC1

int main(void) {
	SystemInit();
	funGpioInitAll();

	SPI_init(TFT_RESET, TFT_DC);
	fun_st7335_init(160, 80, TFT_CS);

	fun_st7735_fill_all(ST_BLACK);
	tft_fill_rect(0, 0, 40, 80, ST_RED);
	tft_fill_rect(40, 0, 40, 80, ST_GREEN);
	tft_fill_rect(80, 0, 40, 80, ST_BLUE);
	tft_fill_rect(120, 0, 40, 80, ST_WHITE);
	tft_print("CH32V003", 4, 34, ST_YELLOW, ST_BLACK);

	while (1) { }
}
