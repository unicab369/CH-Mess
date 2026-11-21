# CH32V003 ST7735 test example

This example initializes a 160×80 ST7735 display and draws text, lines,
rectangles, and a filled triangle. It uses SPI1 on PC5/PC6.

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

Build and flash from this directory with `make flash`.
