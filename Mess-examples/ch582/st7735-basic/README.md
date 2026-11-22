# CH582 ST7735 basic example

This example uses `Mess-libs/spi/fun_st77352.h` and the shared SPI port to
initialize a 160×80 ST7735 display, draw four color bands, and print `CH582`.

## Wiring

| ST7735 module | CH582 |
| --- | --- |
| SCL / SCK | PA13 |
| SDA / MOSI | PA14 |
| CS | PB4 |
| DC / A0 | PB23 |
| RES / RESET | PB22 |
| MISO / SDO | Not used |
| LED / BL | 3.3 V (or board's backlight supply) |
| VCC | 3.3 V |
| GND | GND |

Build and flash from this directory with `make flash`. Use `make monitor` to
view ch32fun debug output. Other ST7735 modules may need different dimensions
or display offsets.
