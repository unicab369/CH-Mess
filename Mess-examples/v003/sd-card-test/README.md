# CH32V003 SD card test

This read-only example initializes an SD card over SPI1, reads sector 0, and
tries to mount its existing FAT filesystem. It does not format or write to the
card.

| SD card | CH32V003 |
| --- | --- |
| SCK / CLK | PC5 |
| MOSI / DI | PC6 |
| MISO / DO | PC7 |
| CS | PD3 |
| VCC | 3.3 V |
| GND | GND |

Build and flash from this directory with `make flash`. The example prints
status over the usual ch32fun debug output.
