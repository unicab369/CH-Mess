#include "ch32fun.h"

/* This library's SPI backend uses SPI1: PC5=SCK and PC6=MOSI. */
#include "../../../Mess-libs/spi/spi_port.h"
#include "../../../Mess-libs/spi/fun_st77352.h"

#define TFT_CS    PA1
#define TFT_DC    PD4
#define TFT_RESET PA2

int main(void) {
	SystemInit();
	funGpioInitAll();

	SPI_INIT(TFT_RESET, TFT_DC);
	SPI_RESET(TFT_RESET, 1);
	st7735_init(160, 80, TFT_CS);

	st7735_fill(ST_BLACK);
	tft_fill_rect(0, 0, 40, 80, ST_RED);
	tft_fill_rect(40, 0, 40, 80, ST_GREEN);
	tft_fill_rect(80, 0, 40, 80, ST_BLUE);
	tft_fill_rect(120, 0, 40, 80, ST_WHITE);
	tft_print("CH32V003", 4, 34, ST_YELLOW, ST_BLACK);

	while (1) { }
}
