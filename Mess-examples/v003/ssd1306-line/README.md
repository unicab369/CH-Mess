# CH32V003 SSD1306 line benchmark

This example compares `prefill_line()` and `ssd1306_drawLine()` using the same
six 1-pixel line segments. It runs 1,000 sets of lines per round for five
rounds per function, measures each function in a separate phase, and prints
average SysTick ticks per line to the serial/debug output.

Both functions draw into the same frame buffer. After timing, the example
sends the frame to the SSD1306 display. The module uses I2C address `0x3C` and
the library's default pins: PC2 for SCL and PC1 for SDA. Build and flash from
this directory with `make flash`.
