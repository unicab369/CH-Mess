# CH32V003 SSD1306 rectangle benchmark

This example compares `prefill_rect()` and `ssd1306_drawRect()` drawing the
same 32×24 outline. It runs 500 rectangles per round for five rounds per
function, varies the rectangle position, measures each function in a separate
phase, and prints average SysTick ticks per rectangle to the serial/debug
output.

`prefill_rect()` uses inclusive end coordinates, so the benchmark passes
width minus one and height minus one in its `Area`. Both functions use the
same frame buffer, which is displayed after the measurements.

The SSD1306 module uses I2C address `0x3C` and the library's default pins:
PC2 for SCL and PC1 for SDA. Build and flash from this directory with
`make flash`.
