# CH32V003 SSD1306 horizontal-line benchmark

This example compares `prefill_horLine()` with `ssd1306_drawFastHLine()` on
the same 120-pixel horizontal line. It tests 1,000 lines per round for five
rounds, varies the line's y coordinate, alternates which function runs first,
and prints average SysTick ticks per call to the serial/debug output.

Both functions use the same frame buffer. After timing, the example sends the
buffer to the SSD1306 display. The module uses I2C address `0x3C` and the
library's default pins: PC2 for SCL and PC1 for SDA. Build and flash from this
directory with `make flash`.
