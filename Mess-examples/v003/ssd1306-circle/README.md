# CH32V003 SSD1306 circle benchmark

This example benchmarks outline and filled circles using `render_circle()`.
Each benchmark uses the same radius and varying centers. It runs 500 circles
per round for five rounds per mode and prints average SysTick ticks per circle
to the serial/debug output.

After timing, the display shows an outline circle above a filled circle. The
OLED uses I2C address `0x3C` and the library's default pins: PC2 for SCL and
PC1 for SDA. Build and flash from this directory with `make flash`.
