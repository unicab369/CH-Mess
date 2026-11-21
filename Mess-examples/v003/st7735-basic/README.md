# CH32V003 ST7735 basic example

This example uses `Mess-libs/spi/fun_st77352.h` and its `lib_tft` drawing
helpers to initialize a 160×80 ST7735 SPI display, draw four color bands, and
print `CH32V003`.

## Wiring

| ST7735 module | CH32V003 |
| --- | --- |
| SCL / SCK | PC5 |
| SDA / MOSI | PC6 |
| CS | PA1 |
| DC / A0 | PD4 |
| RES / RESET | PA2 |
| MISO / SDO | Not used |
| LED / BL | 3.3 V (or board's backlight supply) |
| VCC | 3.3 V |
| GND | GND |

The library's SPI backend is hard-coded to SPI1 on PC5/PC6. This example is
written for a 160×80 panel; other ST7735 modules may need different dimensions
or offsets.

Build and flash from this directory with `make flash`. Use `make monitor` if
you want to view ch32fun debug output.
