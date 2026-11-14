# Water Temperature Range

At standard atmospheric pressure, water freezes at `0 °C` (`32 °F`) and boils at `100 °C` (`212 °F`).

| Point | Celsius | Fahrenheit |
|---|---:|---:|
| Lowest recorded air temperature (Vostok, Antarctica; 1983) | −89.2 °C | −128.6 °F |
| Freezing | 0 °C | 32 °F |
| Highest recorded air temperature (Furnace Creek, California; 1913) | 56.7 °C | 134.0 °F |
| Boiling | 100 °C | 212 °F |
| Wildfire flame temperature (maximum may approach) | ~1,600 °C | ~2,912 °F |
| Natural gas (methane) flame, approximate maximum in air | ~1,950 °C | ~3,542 °F |
| Propane flame, approximate maximum in air | ~1,980 °C | ~3,596 °F |

## Temperature Ranges

| Range | Celsius | Fahrenheit | 1-byte resolution | 1.5-byte resolution | 2-byte resolution |
|---|---:|---:|---:|---:|---:|
| Freezing to highest recorded air temperature | 56.7 °C | 102 °F | 0.222 °C<br>0.400 °F | 0.014 °C<br>0.025 °F | 0.001 °C<br>0.002 °F |
| Freezing to boiling | 100 °C | 180 °F | 0.392 °C<br>0.706 °F | 0.024 °C<br>0.044 °F | 0.002 °C<br>0.003 °F |
| Freezing to wildfire flame temperature | 1,600 °C | 2,880 °F | 6.275 °C<br>11.294 °F | 0.391 °C<br>0.703 °F | 0.024 °C<br>0.044 °F |
| Freezing to natural gas flame temperature | 1,950 °C | 3,510 °F | 7.647 °C<br>13.765 °F | 0.476 °C<br>0.857 °F | 0.030 °C<br>0.054 °F |
| Freezing to propane flame temperature | 1,980 °C | 3,564 °F | 7.765 °C<br>13.976 °F | 0.484 °C<br>0.870 °F | 0.030 °C<br>0.054 °F |
| Lowest to highest recorded air temperature | 146 °C | 263 °F | 0.572 °C<br>1.030 °F | 0.036 °C<br>0.064 °F | 0.002 °C<br>0.004 °F |
| Lowest recorded air temperature to boiling | 189 °C | 341 °F | 0.742 °C<br>1.336 °F | 0.046 °C<br>0.083 °F | 0.003 °C<br>0.005 °F |
| Lowest recorded air temperature to propane flame temperature | 2,069 °C | 3,725 °F | 8.115 °C<br>14.606 °F | 0.505 °C<br>0.910 °F | 0.032 °C<br>0.057 °F |

Resolution assumes unsigned codes map linearly from the range's low endpoint to its high endpoint. The 1-byte, 1.5-byte, and 2-byte columns use 256, 4,096, and 65,536 code values respectively. A 1.5-byte value requires 12-bit packing; the current `col_widths` format only allocates whole bytes.
