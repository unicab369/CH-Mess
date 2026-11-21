# CH582 SD card example

This example uses the `sd_card` library's SPI-mode SD driver and Petit FatFs
to mount a FAT-formatted card, write `HELLO.TXT`, then open and print it.

## Wiring

Connect a 3.3 V SD card module to the CH582 SPI1 pins used by the driver:

| SD card | CH582 |
| --- | --- |
| SCK / CLK | PC5 |
| MOSI / DI | PC6 |
| MISO / DO | PC7 |
| CS | PC4 |
| VCC | 3.3 V |
| GND | GND |

Use a FAT12/16/32 formatted card. The example writes to the root directory;
it expects the card to be writable.

Build with `make` and program with `make flash` from this directory. The
example reports each operation and its Petit FatFs result over the usual
ch32fun debug output.
