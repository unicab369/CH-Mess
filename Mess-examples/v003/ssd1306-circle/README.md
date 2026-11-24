# CH32V003 SSD1306 circle benchmark

This example compares outline circles using `render_circle()` and
`ssd1306_drawCircle()`, plus filled circles using `render_circle()` and
`ssd1306_fillCircle()`. Each benchmark uses the same radius and varying
centers. It runs 500 circles per round for five rounds per function and prints
average SysTick ticks per circle to the serial/debug output.

All methods use the same frame buffer. After timing, the example displays one
`render_circle()` outline centered on the screen. The OLED uses I2C address
`0x3C` and the library's default pins: PC2 for SCL and PC1 for SDA. Build and
flash from this directory with `make flash`.
