# CH32V003 SSD1306 rectangle benchmark

This example compares outline drawing with `render_rect()` and
`ssd1306_drawRect()`, then filled rectangles with `render_rect()` and
`ssd1306_fillRect()`. Each pair draws the same 32×24 rectangles at the same
varying positions. It runs 500 rectangles per round for five rounds per
function, measures each function in a separate phase, and prints average
SysTick ticks per rectangle to the serial/debug output.

`render_rect()` uses inclusive end coordinates, so the benchmark passes
width minus one and height minus one as its width and height offsets. Both
implementations use the same frame buffer. The display shows the accumulated
filled rectangles from the final benchmark phase.

The SSD1306 module uses I2C address `0x3C` and the library's default pins:
PC2 for SCL and PC1 for SDA. Build and flash from this directory with
`make flash`.
