# CH32V003 SSD1306 loading bar

This example animates a filled loading bar on an SSD1306 I2C display. Set
`ssd1306_render_loading_progress()` takes a start point, height, full width, and
filled width, all measured in pixels.

The bar is drawn with `ssd1306_render_rect()` and displayed through the
SSD1306 I2C library. Build and flash from this directory with `make flash`.
