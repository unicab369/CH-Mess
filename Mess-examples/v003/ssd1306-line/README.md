# CH32V003 SSD1306 line benchmark

This example benchmarks `render_line()` with six 1-pixel line segments and
with a 3-pixel diagonal. Each case runs 100 sets per round for five rounds and
prints average SysTick ticks per line to the serial/debug output.

Both functions draw into the same frame buffer. After timing, the example
sends the frame to the SSD1306 display. The module uses I2C address `0x3C` and
the library's default pins: PC2 for SCL and PC1 for SDA. Build and flash from
this directory with `make flash`.
