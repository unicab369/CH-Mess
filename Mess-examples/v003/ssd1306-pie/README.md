# CH32V003 SSD1306 pie example

Draws four filled 90-degree sectors with `render_pie()` on a 128×64 SSD1306.
The sectors show the four angle quadrants. The module uses I2C address `0x3C`
and the library's default pins: PC2 for SCL and PC1 for SDA.

Build and flash from this directory with `make flash`.
