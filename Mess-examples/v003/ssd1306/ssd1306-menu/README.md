# CH32V003 SSD1306 text menu

This example displays a text menu on an SSD1306 I2C display. The `>` marker
moves to the next item once per second to demonstrate changing the selection.

The library's default I2C pins are PC2 for SCL and PC1 for SDA.

| SSD1306 module | CH32V003 |
| --- | --- |
| SCL | PC2 |
| SDA | PC1 |
| VCC | 3.3 V |
| GND | GND |

Build and flash from this directory with `make flash`.
