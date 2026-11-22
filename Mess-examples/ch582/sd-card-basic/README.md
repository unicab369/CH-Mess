# CH582 SD card basic test

This example initializes the SD card, reads sector 0, mounts an existing FAT
filesystem, creates or opens `HELLO.TXT`, writes a string, then reads it back
and compares it. It does not format the card. The card must already have a FAT
filesystem; Petit FatFs supports FAT16 and FAT32 here.

## Wiring

| SD card | CH582 |
| --- | --- |
| SCK / CLK | PA13 |
| MOSI / DI | PA14 |
| MISO / DO | PA15 |
| CS | PB7 |
| VCC | 3.3 V |
| GND | GND |

Build and flash from this directory with `make` and `make flash`. The example
reports progress over ch32fun debug output.
