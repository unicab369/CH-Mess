# CH582 SD card example

This example uses `Mess-libs/spi/sd_card` over the shared SPI port and Petit FatFs
to format a 256 MiB FAT16 partition, mount it, write `HELLO.TXT`, then open
and print it. **Formatting runs on every reset and erases existing card data.**
Cards must have at least 257 MiB of capacity; any additional capacity is left
unused by this demo.

## Wiring

Connect a 3.3 V SD card module to CH582 SPI0's default pins:

| SD card | CH582 |
| --- | --- |
| SCK / CLK | PA13 |
| MOSI / DI | PA14 |
| MISO / DO | PA15 |
| CS | PA5 |
| VCC | 3.3 V |
| GND | GND |

The example creates the filesystem itself. It writes to the root directory.

Build with `make` and program with `make flash` from this directory. The
example reports each operation and its Petit FatFs result over the usual
ch32fun debug output.
