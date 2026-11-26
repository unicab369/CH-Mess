# CH32V003 SSD1306 loading bar

This example animates a filled loading bar on an SSD1306 I2C display. Set
`ssd1306_show_progress()` takes the bar's x/y position, height, width, pixel
progress, and direction (`DISPLAY_LANDSCAPE` or `DISPLAY_PORTRAIT`).
`ssd1306_show_percent()` accepts the same dimensions with progress from 0 to
100 percent. Both update only the display area touched by the bar. Call
`ssd1306_reset_progress(x, y, width, height, direction)` to clear a bar before
starting over. The example displays a horizontal bar at the top and a vertical
bar below it.

The bar is drawn with `ssd1306_render_rect()` and displayed through the
SSD1306 I2C library. Build and flash from this directory with `make flash`.
