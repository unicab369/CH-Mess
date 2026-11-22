#include "ch32fun.h"

#define TFT_CS    PB4
#define TFT_DC    PB10
#define TFT_RESET PB22

#include "../../../Mess-libs/spi/spi_port.h"
#include "../../../Mess-libs/spi/fun_st77352.h"

int main(void) {
	SystemInit();
	SPI_INIT();
	// R8_SPI0_CLOCK_DIV = 60; // Conservative 1 MHz SPI clock at 60 MHz system clock.
	SPI_CONFIGURE_PINS(TFT_RESET, TFT_DC);
	SPI_RESET(TFT_RESET, 100);
	st7735_init(160, 80, TFT_CS);

	st7735_fill(ST_BLACK);
	tft_fill_rect(0, 0, 40, 80, ST_RED);
	tft_fill_rect(40, 0, 40, 80, ST_GREEN);
	tft_fill_rect(80, 0, 40, 80, ST_BLUE);
	tft_fill_rect(120, 0, 40, 80, ST_WHITE);
	tft_print("CH582", 4, 34, ST_YELLOW, ST_BLACK);

	while (1) { }
}
