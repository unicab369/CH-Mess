# CH32V003 SSD1306 loading bar

This example animates a filled loading bar on an SSD1306 I2C display. Set
`ssd1306_show_progress()` takes the bar's x/y position, height, full width, and
progress in pixels. `ssd1306_show_percent()` accepts the
same dimensions with progress from 0 to 100 percent. Both update only the
display pages touched by the bar. Progress is incremental for one bar; call
`ssd1306_reset_progress(x, y, height)` to clear it before starting over.

The bar is drawn with `ssd1306_render_rect()` and displayed through the
SSD1306 I2C library. Build and flash from this directory with `make flash`.
