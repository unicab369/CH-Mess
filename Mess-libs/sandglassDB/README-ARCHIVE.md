# Water Temperature Range

At standard atmospheric pressure, water freezes at `0 °C` (`32 °F`) and boils at `100 °C` (`212 °F`).

| Point | Celsius | Fahrenheit |
|---|---:|---:|
| Lowest recorded air temperature (Vostok, Antarctica; 1983) | −89.2 °C | −128.6 °F |
| Lowest recorded air temperature in the United States (Prospect Creek, Alaska; 1971) | −62.2 °C | −80 °F |
| Freezing | 0 °C | 32 °F |
| Highest recorded air temperature (Furnace Creek, California; 1913) | 56.7 °C | 134.0 °F |
| Boiling | 100 °C | 212 °F |
| Wildfire flame temperature (maximum may approach) | ~1,600 °C | ~2,912 °F |
| Natural gas (methane) flame, approximate maximum in air | ~1,950 °C | ~3,542 °F |
| Propane flame, approximate maximum in air | ~1,980 °C | ~3,596 °F |

## Temperature Ranges

| Range | Fahrenheit | 1-byte resolution | 2-byte resolution |
|---|---:|---:|---:|
| Freezing to highest recorded air temperature | 102 °F | 0.400 °F | 0.002 °F |
| Freezing to boiling | 180 °F | 0.706 °F | 0.003 °F |
| Freezing to wildfire flame temperature | 2,880 °F | 11.294 °F | 0.044 °F |
| Freezing to natural gas flame temperature | 3,510 °F | 13.765 °F | 0.054 °F |
| Freezing to propane flame temperature | 3,564 °F | 13.976 °F | 0.054 °F |
| Lowest to highest recorded air temperature | 263 °F | 1.030 °F | 0.004 °F |
| Lowest recorded air temperature to boiling | 341 °F | 1.336 °F | 0.005 °F |
| Lowest recorded air temperature to propane flame temperature | 3,725 °F | 14.606 °F | 0.057 °F |

Resolution assumes unsigned codes map linearly from the range's low endpoint to its high endpoint. The 1-byte and 2-byte columns use 256 and 65,536 code values respectively.

## Uniform Quantization Example

For a temperature range of −40.0 to 120.0 °F, multiply by 10 to express values in tenths: −400 to 1200. Add 400 to normalize the integer range to 0–1600. This gives 1,601 distinct values, so storing every 0.1 °F step exactly requires 11 bits.

With 8-bit uniform quantization, 256 codes span the same range. Adjacent decoded levels are `160 °F / 255 ≈ 0.627 °F` apart, with up to about 0.314 °F error when rounding to the nearest level. A 1-byte code therefore saves space but does not preserve every 0.1 °F value.

If a Record block stores the average temperature of its readings, each reading can instead be encoded as a difference from that average. Assuming every difference lies between −20 and +20 °F, a 6-bit code provides 64 levels across 40 °F: `40 °F / 63 ≈ 0.635 °F` between decoded levels, with up to about 0.317 °F rounding error. This still cannot preserve every 0.1 °F value.

With 32-bit alignment, 5 readings use 30 bits, leaving 2 unused bits per 32-bit word. That is 4 bytes for 5 readings, or 6.4 bits per reading including the unused bits.

The final block average is unknown while readings are being appended. This method requires buffering the readings until finalization or using a reference temperature chosen before writing. Continuation blocks would also need a defined reference if they use the same encoding.


Monthly * 240 = ~20 years<br>
Weekly * 960 = ~20 years<br>
Daily * 361 = ~1 year<br>
Hourly * 240 = ~10 days<br>
10 mins * 720 = ~10 days<br>
1 mins * 1440 = 1 day<br>
5 secs * 720 = 1 hour<br>
1 sec * 3600 = 1 hour
<br>
