# Archiving and Temperature Encoding

## Live and Archive Encoding Plan

Each schema selects its value representation and compression steps through the `codec` byte described in [Codec Byte](README.md#codec-byte). A live stream can use a raw or uniformly quantized schema. After finalizing a source block and producing readings for a longer interval, an archive stream can select its own schema and codec. To change a codec within one dataset, start a new `R` with a new `schema_id`; its `C` blocks use that schema. Raw fixed-point temperature values can serve as a comparison baseline for quantized live readings; match samples by timestamp when they are written in separate streams.

Uniform quantization maps a configured value range to evenly spaced integer codes. Delta records changes between consecutive values; delta-of-delta records changes between consecutive deltas. RLE records repeated predictor results with a count. The lossless steps can be combined with either raw values or quantized codes. For example:

| Codec | Representation | Predictor | RLE |
|---|---|---|---|
| `0x00` | Raw | None | Off |
| `0x01` | Uniform quantization | None | Off |
| `0x02` | Raw | Delta | Off |
| `0x03` | Uniform quantization | Delta | Off |
| `0x0B` | Uniform quantization | Delta | On |
| `0x10` | Segmented 2 B or 4 B base + signed 1 B offsets | Block base | Off |

`0x00` and `0x10` have defined payload formats; the other rows are candidates. Compare candidate combinations on representative readings before defining the other formats. In particular, record the total bytes per block, decoding work, quantization error, and the effect of out-of-range values. A uniform range and bit width must be stored in the schema before quantized blocks can be written.

### Segmented Temperature Example

For `codec = 0x10`, use a temperature schema with `value_width = 2`, signed values, and a scale of 0.1 °F per integer. The same `R` and `C` headers apply. Each block payload contains one or more segments; other schemas may use a 4 B base:

```text
[time_offset: 2 B unsigned][reading_count: 2 B][base_value: 2 B][value_offsets: 1 B each]
```

The first segment uses `time_offset = 0`; later segments use unsigned time offsets from the block's `base_time` when timing skips or moves backward relative to a previous segment. A segment cannot start before `base_time`. For example, `base_value = 700` represents 70.0 °F. Each later reading stores one signed 1 B value offset from that base, from −127 to +127 tenths of a degree, so this segment can represent 57.3–82.7 °F. The `-128` code is reserved. Readings 70.0, 70.1, 70.2, and 70.1 °F store `base_value = 700`, `reading_count = 4`, and `value_offsets = [+1, +2, +1]`. Start a new segment when a reading falls outside the range. The block header's `payload_length` marks the end of the segments; add their `reading_count` values to obtain the total reading count.

Other lossless methods to compare include:

| Candidate | Stored information | Best case |
|---|---|---|
| Shared upper bits | Common high bits once, then low bits per reading | All codes stay within one aligned range. |
| Base + packed offsets | A base code once, then small differences from that base | Codes stay within a narrow range, even across a high-bit boundary. |
| Delta encoding | First code, then differences between consecutive codes | Consecutive codes change very little. |

For example, 8-bit codes `0101 0010`, `0101 0011`, and `0101 0100` use 24 bits raw. Storing the shared `0101` once and the low parts `0010`, `0011`, `0100` uses 16 bits before format overhead. A code that crosses the shared-bit boundary needs a new group or an escape rule. Base + packed offsets can avoid that boundary problem.

These methods can preserve the input codes exactly. Mapping temperatures to fewer codes first is uniform quantization and introduces rounding error. Define each compressed payload layout, predictor seed, run format, decoded count rule, and decoding rules before writing blocks with that codec.

## Water Temperature Range

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

For the offset options, a Record block stores an average temperature and each reading stores a difference from it. The other options encode the full temperature range directly. The rounding error assumes decoding to the nearest level.

| Bits per reading | Decoded levels | Encoded range | Maximum rounding error (°F) | Readings per 32-bit word | Unused bits per word |
|---:|---|---:|---:|---:|---:|
| 6 | 64 | −20 to +20 °F from block average | 0.317 °F | 5 | 2 |
| 7 | 128 | −20 to +20 °F from block average | 0.157 °F | 4 | 4 |
| 8 | 256 | −20 to +20 °F from block average | 0.078 °F | 4 | 0 |
| 8 | 256 | −40 to 120 °F | 0.314 °F | 4 | 0 |
| 10 | 1,024 | −40 to 120 °F | 0.078 °F | 3 | 2 |

The final block average is unknown while readings are being appended. This method requires buffering the readings until finalization or using a reference temperature chosen before writing. Continuation blocks would also need a defined reference if they use the same encoding.

### Powers of Two

| `n` | `2ⁿ` | Signed range (two's complement) |
|---:|---:|---:|
| 4 | 16 | −8 to +7 |
| 5 | 32 | −16 to +15 |
| 6 | 64 | −32 to +31 |
| 7 | 128 | −64 to +63 |
| 8 | 256 | −128 to +127 |
| 9 | 512 | −256 to +255 |
| 10 | 1,024 | −512 to +511 |
| 11 | 2,048 | −1,024 to +1,023 |
| 12 | 4,096 | −2,048 to +2,047 |
| 13 | 8,192 | −4,096 to +4,095 |
| 14 | 16,384 | −8,192 to +8,191 |
| 15 | 32,768 | −16,384 to +16,383 |
| 16 | 65,536 | −32,768 to +32,767 |
| 24 | 16,777,216 | −8,388,608 to +8,388,607 |

## Time Range by Interval

This table shows the duration represented by each number of interval steps.

| Number of steps | 1 ns | 1 μs | 1 ms | 1 second | 1 minute | 1 hour | 1 day |
|---:|---:|---:|---:|---:|---:|---:|---:|
| `2⁸ = 256` | 256 ns | 256 μs | 256 ms | 4.3 minutes | 0.18 days | 10.67 days | 256 days |
| `2¹² = 4,096` | 4.10 μs | 4.10 ms | 4.10 s | 1.14 hours | 2.84 days | 170.67 days | ~11 years |
| `2¹⁶ = 65,536` | 65.54 μs | 65.54 ms | 1.09 minutes | 18.2 hours | 45.51 days | ~8 years | ~179 years |
| `2¹⁸ = 262,144` | 262.14 μs | 262.14 ms | 4.37 minutes | ~3.0 days | ~182.0 days | ~30 years |  |
| `2²⁰ = 1,048,576` | 1.05 ms | 1.05 s | 17.48 minutes | ~12.1 days | ~2 years | ~120 years |  |
| `2²⁴ = 16,777,216` | 16.78 ms | 16.78 s | ~4.66 hours | ~194.2 days | ~32 years |  |  |
| `2³² = 4,294,967,296` | ~4.29 seconds | ~71.6 minutes | ~49.7 days | ~136 years | ~8,166 years |  |  |

There are **86,400 seconds in a day** (`24 × 60 × 60`).

Monthly * 240 = ~20 years<br>
Weekly * 960 = ~20 years<br>
Daily * 361 = ~1 year<br>
Hourly * 240 = ~10 days<br>
10 mins * 720 = ~10 days<br>
1 mins * 1440 = 1 day<br>
5 secs * 720 = 1 hour<br>
1 sec * 3600 = 1 hour
<br>
