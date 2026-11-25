# CH32V003 SSD1306 polygon example

Runs the polygon drawing demo formerly in `test_polys()` in the SSD1306
header. It shows filled and outline polygons plus small rectangles.

The display uses I2C address `0x3C` and the library's default pins: PC2 for
SCL and PC1 for SDA. Build and flash from this directory with `make flash`.
