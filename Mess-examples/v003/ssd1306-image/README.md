# CH32V003 SSD1306 image benchmark

Compares `render_image()` with `ssd1306_drawImage()` using the same 32×32
monochrome bitmap. Each result averages `BENCH_RUNS` calls per round over five
rounds. The display shows the rendered bitmap after printing the timings.

Build and flash from this directory with `make flash`.
