# CH32V003 SSD1306 I2C basic example

This example initializes an SSD1306 128×64 display at I2C address `0x3C`,
then draws an alternating pattern on the first page and clears the rest. It
uses the CH32V003 hardware I2C library in `Mess-libs/i2c`.

The library's default I2C pins are PC2 for SCL and PC1 for SDA.

| SSD1306 module | CH32V003 |
| --- | --- |
| SCL | PC2 |
| SDA | PC1 |
| VCC | 3.3 V |
| GND | GND |

Use a module with I2C interface and address `0x3C`. Build and flash from this
directory with `make flash`.
