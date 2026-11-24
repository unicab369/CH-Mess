# CH32V003 SSD1306 horizontal-line benchmark

This example compares a 1-pixel `render_horLine()` with one
`ssd1306_drawFastHLine()` call. It also compares a 2-pixel-thick
`render_horLine()` against two `ssd1306_drawFastHLine()` calls on adjacent
rows, so both draw the same thickness. Each case runs 1,000 lines per round
for five rounds, varies the line's y coordinate, and prints average SysTick
ticks per line to the serial/debug output.

Both functions use the same frame buffer. After timing, the example sends the
buffer to the SSD1306 display. The module uses I2C address `0x3C` and the
library's default pins: PC2 for SCL and PC1 for SDA. Build and flash from this
directory with `make flash`.
