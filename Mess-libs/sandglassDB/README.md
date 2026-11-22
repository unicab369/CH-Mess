# SandglassDB

## Overview

The database starts with 2 Superblocks (`S0`, `S1`); all remaining flash sectors form a shared storage pool. A Record block (`R`) starts an extent, and Continuation blocks (`C`) extend it when its payload fills. Each `dataset_id` identifies one stream; its `R` block specifies the `interval_code` and `interval_value`. Blocks from different datasets may interleave. Each `C` belongs to the most recent preceding `R` with the same `dataset_id`.

Each dataset stores one measurement type, such as temperature or humidity. One reading is one value in an `R` or `C` payload. The extent's schema defines that value's width, signedness, unit, scale, and codec. Live and archive datasets can use different schemas; see [Archiving](README-ARCHIVE.md) for an encoding example.

```text
[S0] [S1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Each bracket represents one database block occupying one 4 KiB flash sector.


For this flash layout, each erase sector is 4,096 B. SandglassDB stores one logical block per sector. A block's header and remaining space share those 4,096 B.

When a database is accessed, it needs at least two things:

- **Magic number:** Identifies the start of a header and indicates that the information for that header begins at this address.
- **CRC checksum:** Detects accidental changes or corruption in raw data. A mathematical operation reduces each set of data—for example, 250 bytes to a smaller checksum. Compare the checksums for 2 sets to check whether the data is the same, **`without comparing all 250 bytes one by one`**:

  ```text
  Set 1: 250 bytes of data → 2-byte checksum 1
  Set 2: 250 bytes of data → 2-byte checksum 2
  ```

  If the checksums match, the data is likely the same; if they differ, the data is different or corrupted.

> In the field tables, `*` after a size means its length is adjustable.

## Superblocks (`S0`, `S1`)

The 2 fixed blocks used to configure the database are copies of each other. If one block is corrupted, for example, its **`magic number`** or **`CRC`** does not match, the other block can be used to recover the configuration.

The Superblock also needs a `version` number. It identifies the expected format and allows the format to change over time, such as from version 1 to version 2.

Each Superblock contains:

| field | size | purpose |
|---|---:|---|
| **crc** | 4 B | Covers the fixed header and alignment padding, excluding the CRC field itself and `schema_entries`. |
| **magic_number** | 2 B | Identifies the start of the Superblock header. |
| **version** | 1 B | Identifies the Superblock format. |
| **config** | 1 B | Stores database-wide configuration options. |
| **generation** | 1 B | Identifies the newer fixed Superblock header; wraps from `255` to `0`. |
| **alignment_padding** | 3 B | Set to `0`; aligns `schema_entries` to a 4-byte boundary. |
| **schema_entries** | 4,084 B | Append-only schema entries. Each entry has its own CRC and `commit` field; see [Schema Entries](#schema-entries) for the entry layout. |

The Superblock is stored in 2 copies, `S0` and `S1`. Each copy has this field layout:
```text
       +-----+--------------+---------+--------+------------+-------------------+----------------+
S0:    | crc | magic_number | version | config | generation | alignment_padding | schema_entries |
       +-----+--------------+---------+--------+------------+-------------------+----------------+
S1:    | crc | magic_number | version | config | generation | alignment_padding | schema_entries |
       +-----+--------------+---------+--------+------------+-------------------+----------------+
```

## Schema Entries

Schema updates append entries to unused space in both Superblocks. Each update gets a new `schema_id`; existing entries and IDs are never changed or reused. An `R` may use the new ID only after its entry is committed and valid in `S0` and `S1`.

The application selects the `schema_id` for each new `R` block. Adding a schema does not change existing streams. For example, a stream can use `schema_id = 4` in its next `R`; subsequent `C` blocks in that stream also use schema 4.

To change a dataset's codec, append a new schema entry with the desired codec and start a new `R` using its `schema_id`. The new entry may keep the same value definition. Existing `C` blocks continue using their parent `R`'s schema.

The CRC for the fixed Superblock fields does not cover the append-only entries; each entry has its own CRC and `commit` field. Readers scan the entries in both copies and build a lookup from `schema_id` to payload layout.

Each schema entry describes one value type for a dataset. Its fields appear in this order:

| Field | Size | Purpose |
|---|---:|---|
| **entry_crc** | 4 B | Validates the schema entry. |
| **commit** | 1 B | Marks the entry as fully written. |
| **schema_id** | 1 B | Identifies the schema entry. |
| **time_format** | 1 B | Defines the 4 B timestamps in raw payloads and each Segment's `base_time`; format codes still need definition. |
| **codec** | 1 B | Selects how this value is stored; see [Codec Byte](#codec-byte). |
| **value_width** | 1 B | Number of bytes in a decoded raw value (`1`–`8`). |
| **value_unit** | 1 B | Unit code for the value; see [Unit Codes](#unit-codes). |
| **value_config** | 1 B | Sets signedness, byte order, and rounding; see [value_config](#value_config). |
| **scale_config** | 2 B | Sets the sizes of scale and bound fields; see [scale_config](#scale_config). |
| **value_scale_exp** | 0, 1, 2, or 4 B* | Signed base-10 exponent; if omitted (0 B), use `0`. |
| **value_scale_numerator** | 0, 1, 2, or 4 B* | Unsigned numerator; if omitted (0 B), use `1`. |
| **value_scale_denominator** | 0, 1, 2, or 4 B* | Unsigned denominator; if omitted (0 B), use `1`. Must be nonzero when stored. |
| **value_bias** | 0, 1, 2, or 4 B* | Signed constant added after the ratio and before the exponent; if omitted (0 B), use `0`. |
| **value_min** | 0, 1, 2, 4, or 8 B* | Lowest permitted stored integer before scaling; omitted with `value_max` when no custom bounds are needed. |
| **value_max** | 0, 1, 2, 4, or 8 B* | Highest permitted stored integer before scaling; uses the same width as `value_min`. |
| **padding** | Variable | Aligns the entry to the flash program unit. |

### `value_config`

| Bit(s) | Meaning | Purpose |
|---|---|---|
| 7 | Reserved; write `0`. | Reserved for future options. |
| 6 | `byte_order`: `0` = little-endian; `1` = big-endian. | Sets byte order for multi-byte value and scale fields. |
| 5–3 | Reserved; write `0`. | Reserved for future options. |
| 2–1 | `rounding_mode`:<br>`11` = upward (toward +∞)<br>`10` = downward (toward −∞)<br>`01` = truncate to an integer<br>`00` = nearest, ties to even | Rounds input values before storage. |
| 0 | `signed`: `1` = signed; `0` = unsigned. | Selects how the value is interpreted. |

`rounding_mode` applies when converting an input value to a stored integer or quantization code. It does not change how readers decode an already-stored value or how applications round for display.

After converting a measurement into storage units, values between integers round as follows:

| Value | `00` nearest | `01` truncate | `10` downward | `11` upward |
|---:|---:|---:|---:|---:|
| `2.7` | `3` | `2` | `2` | `3` |
| `−2.7` | `−3` | `−2` | `−3` | `−2` |
| `2.5` | `2` | `2` | `2` | `3` |
| `3.5` | `4` | `3` | `3` | `4` |

### `scale_config`

`scale_config` is stored as a 2 B little-endian value, regardless of `value_config` byte order. It assigns size codes to the bounds, bias, exponent, and ratio. Each pair of bounds or ratio components shares one width:

| Bit(s) | Component |
|---:|---|
| 15–9 | Reserved; write `0`. |
| 8–6 | Shared width of `value_min` and `value_max`:<br>`100` = 8 B<br>`011` = 4 B<br>`010` = 2 B<br>`001` = 1 B<br>`000` = 0 B (both omitted)<br>`101`–`111` = reserved. |
| 5–4 | `value_bias` width. |
| 3–2 | `value_scale_exp` width. |
| 1–0 | Shared width of `value_scale_numerator` and `value_scale_denominator`. |

| 2-bit size code | Byte width |
|---:|---:|
| `11` | 4 B |
| `10` | 2 B |
| `01` | 1 B |
| `00` | 0 B (component omitted) |

An omitted exponent or bias defaults to `0`; an omitted numerator and denominator each default to `1`.

### Codec Byte

Each schema entry stores a 1-byte `codec`. An `R` selects the schema for its extent, and its `C` blocks inherit that codec. The proposed bits separate the stored value representation from the lossless compression steps:

| Bit(s) | Meaning | Purpose |
|---:|---|---|
| 7–5 | Reserved; write `0`. | Leaves room for future encodings. |
| 4 | `segmented_offsets`: `1` = segmented base and offsets; `0` = normal codec bits. | Selects the segmented payload when bits 3–0 are `0`. |
| 3 | `rle`: `0` = off; `1` = on. | Run-length encodes repeated results after prediction. |
| 2–1 | `predictor`: `00` = none; `01` = delta; `10` = delta-of-delta; `11` = reserved. | Encodes changes between consecutive values. |
| 0 | `representation`: `0` = raw; `1` = uniform quantization. | Selects the values passed to the predictor. |

For example, `0x00` means raw, `0x01` means uniform quantization, `0x03` means quantization plus delta, and `0x0B` adds RLE to that combination. `0x10` selects segmented base and offsets; no other combination with bit 4 is valid. A reader rejects reserved or unsupported combinations.

For codecs with bit 4 set to `0`, encode in this order: raw values or uniform quantization → predictor → RLE → payload. Decode in reverse order. Delta starts with one unpredicted value; delta-of-delta starts with two. Predictor state resets at each block boundary, so every block decodes independently. RLE operates on the predictor results; its run counts determine the decoded reading count. Delta and RLE preserve their input exactly; uniform quantization may introduce rounding error.

Uniform quantization needs a range and bit width in the schema. The precise metadata fields, bit packing, signed residual encoding, RLE run format, and out-of-range rule still need definition. The payloads for `codec = 0x00` and `codec = 0x10` are defined below; readers reject other values until their formats are specified. See [Archiving](README-ARCHIVE.md) for how to compare the combinations.

### Unit Codes

`value_unit` is 1 B and uses these codes:

| Codes | Units, in code order |
|---:|---|
| `30–32` | Hertz (Hz), kilohertz (kHz), gigahertz (GHz) |
| `23–29` | Second, Minute, Hour, Day, Calendar month, Calendar year, Week |
| `20–22` | Nanosecond, Microsecond, Millisecond |
| `17–19` | Ounce, Pound, Short ton (US) |
| `13–16` | Inch, Foot, Yard, Mile |
| `10–12` | Kelvin (K), Celsius (°C), Fahrenheit (°F) |
| `5–9` | Lux, Decibel milliwatt (dBm), Decibel (dB), Degree (°), Radian |
| `3–4` | Percent (%), Parts per million (ppm) |
| `2` | Thaums |
| `0–1` | Unspecified, Count |

For example, a temperature schema can use `value_unit = 12` (°F), `value_width = 2`, `value_scale_exp = -1`, and `codec = 0x10`. Each stored integer then represents tenths of a degree Fahrenheit. A new schema entry and `schema_id` are needed when its value format changes.

### Schema Entry Capacity

Each Superblock has 4,084 B for append-only `schema_entries`. An entry has 13 B of fixed fields plus the bounds, bias, exponent, numerator, and denominator selected by `scale_config`. Each pair of bounds or ratio components uses the same width. Entries are padded to 4 B alignment. With 2 B per bound and all 4 scale components using the same width:

| Width per scale component | Aligned entry size | Entries per Superblock | Unused space |
|---:|---:|---:|---:|
| 0 B | 20 B | 204 | 4 B |
| 1 B | 24 B | 170 | 4 B |
| 2 B | 28 B | 145 | 24 B |
| 4 B | 36 B | 113 | 16 B |

With bounds and all scale components omitted, an entry is 16 B and 255 entries fit. The 1 B `schema_id` limits the table to 256 entries even when more physical space is available. The count includes the initial schema; both Superblocks hold the same entries. Other widths change the entry size.

## Streams

Each `dataset_id` identifies one stream. The `R` header stores its `interval_code` and `interval_value`; its `C` blocks store `dataset_id` and inherit the interval from the current `R`. This permits up to 256 dataset IDs.

The application can change a dataset's interval by starting a new `R`, which ends the previous extent for that dataset. Following `C` blocks for the dataset belong to the new `R`. Use separate dataset IDs to write multiple intervals at the same time. See [Interval Codes](#interval-codes).

The library appends all streams to one physical log, so their blocks can interleave. Readers scan the log in physical order and track the current extent for each `dataset_id`.

```text
Physical append order (each row is one 4 KiB sector holding one block):
[R header: dataset_id = 0, interval_code = 4, interval_value = 1] [payload]
[R header: dataset_id = 1, interval_code = 5, interval_value = 5] [payload]
[C header: dataset_id = 0] [payload]
[C header: dataset_id = 1] [payload]
[C header: dataset_id = 0] [payload]

Reader separates the streams:
dataset_id = 0, 1 second: [R] → [C] → [C]
dataset_id = 1, 5 minutes: [R] → [C]
```

`schema_id` is independent of `dataset_id`. Each `R` selects the schema for its extent, and its `C` blocks use that schema. Different streams can use different schemas; a stream can switch schemas by starting a new `R` extent.

## Record blocks (`R`)

Record blocks (`R0`–`Rn`) follow Superblocks `S0` and `S1`. Each record block belongs to the stream identified by its `dataset_id` and uses a `schema_id` whose entry is committed in both Superblocks. Different streams can allocate sectors from the same shared pool for their blocks.

A record block contains:

| field | size | purpose |
|---|---:|---|
| **crc** | 4 B | Covers the header, including `payload_length`, and the used payload; excludes itself. |
| **magic_number** | 2 B | Identifies the start of the record block. |
| **payload_length** | 2 B | Number of valid payload bytes in this block (`0`–`4,084`). |
| **dataset_id** | 1 B | Identifies the dataset. |
| **schema_id** | 1 B | Identifies the schema defining this block's value format. |
| **interval_code** | 1 B | Selects the time unit. |
| **interval_value** | 1 B | Sets the exact gap for fixed-interval raw readings, or the nominal gap for segmented readings. |
| **payload** | 4,084 B remaining | Stores raw readings or encoded segments according to the schema's codec; see [Payload Formats](#payload-formats). |

The Record block header is 12 B. At finalization, write `payload_length`, then compute and store `crc`; readers use the length only if the CRC validates. The `schema_id` selects this extent's codec and `time_format`; its `C` blocks use the same schema and codec.

### Interval Codes

Each `R` stores an `interval_code` and `interval_value` for its extent; its `C` blocks inherit both. For codes `1`–`10`, `interval_value` ranges from `1` to `255`. For code `0`, write `interval_value = 0` because each reading carries its own timestamp. For example, code `5` with value `10` means a 10-minute interval: exact for fixed-interval raw readings and nominal for segmented readings.

| `interval_code` | Time unit |
|---:|---|
| 11–255 | Reserved |
| 10 | Year |
| 9 | Month |
| 8 | Week |
| 7 | Day |
| 6 | Hour |
| 5 | Minute |
| 4 | Second |
| 3 | Millisecond (ms) |
| 2 | Microsecond (μs) |
| 1 | Nanosecond (ns) |
| 0 | Timestamp stored with each reading |

For `interval_code = 0`, store a 4 B timestamp before each raw value, using the schema's `time_format`. For codes `1`–`6`, fixed-interval raw readings derive their timestamps from the payload's `base_time`, `interval_value`, and position; segmented readings use their stored time gaps. Calendar boundaries, time zone, and week start for codes `7`–`10` still need definition.

## Continuation blocks (`C`)

Continuation blocks store records that follow a record block `R` when its payload is full. Each carries the same `dataset_id` and is associated with the current extent for that dataset by physical log order, even when other streams' blocks are interleaved. It inherits `interval_code`, `interval_value`, and `schema_id` from the preceding `R` for that dataset. When a continuation block `C` fills, another continuation block is added. See [Recovery and Log Scanning](#recovery-and-log-scanning) for the ordering rules.

A continuation block contains:

| Field | Size | Purpose |
|---|---:|---|
| **crc** | 4 B | Covers the header, including `payload_length`, and the used payload; excludes itself. |
| **magic_number** | 2 B | Identifies the start of the continuation block. |
| **payload_length** | 2 B | Number of valid payload bytes in this block (`0`–`4,084`). |
| **dataset_id** | 1 B | Identifies the dataset. |
| **alignment_padding** | 3 B | Set to `0`; aligns the payload to a 4-byte boundary. |
| **payload** | 4,084 B remaining | Stores raw readings or encoded segments using the schema of this extent's Record block. |

The Continuation block inherits `interval_code`, `interval_value`, time format, schema, and codec from its parent `R`. Its header is 12 B. For fixed-interval raw payloads, the payload begins with `base_time`; for segmented payloads, every Segment has its own `base_time`. For `interval_code = 0`, each reading stores its own timestamp. At finalization, write `payload_length`, then compute and store `crc`; readers use the length only if the CRC validates.

Raw fixed-interval readings per `C` block, assuming a 12 B header, a 4 B `base_time` prefix, and whole values:

| Value width | Readings per block | Unused payload (B) |
|---:|---:|---:|
| 1 B | 4,080 | 0 |
| 2 B | 2,040 | 0 |
| 4 B | 1,020 | 0 |
| 8 B | 510 | 0 |

> Unused bytes are the payload space remaining after the `base_time` prefix and whole readings. The counts assume readings are packed consecutively with no alignment padding. For `interval_code = 0`, there is no `base_time` prefix; each reading has its own 4 B timestamp.

## Payload Formats

Before checking the CRC, readers reject a `payload_length` greater than `4,084` B. Only the first `payload_length` bytes of the payload are valid and covered by the CRC.

### Raw Readings (`codec = 0x00`)

With codec `0` and a fixed interval, each `R` or `C` stores a 4 B `base_time` followed by raw readings:

```text
Extent example (each row is one 4 KiB sector holding one block):
[R header] [base_time] [reading 0] [reading 1] [reading 2] ...
[C header] [base_time] [reading 0] [reading 1] [reading 2] ...
[C header] [base_time] [reading 0] [reading 1] [reading 2] ...
```

For `interval_code = 1`–`10`, each reading is one `value_width`-byte value after the `base_time` prefix. For `interval_code = 0`, omit the prefix and store each reading as a 4 B timestamp followed by one `value_width`-byte value. At finalization, write the 2 B `payload_length` in the header, then compute and store `crc` over the header and that many payload bytes, excluding the CRC field. For fixed intervals, `payload_length − 4` must be a multiple of `value_width`; for code `0`, `payload_length` must be a multiple of `4 + value_width`. The quotient is the reading count. Remaining payload bytes are unused. Other codecs must define their own byte layouts and decoded reading counts.

### Segmented Base and Offsets (`codec = 0x10`)

This codec requires `value_width = 2`, `4`, `6`, or `8`, signed values (`value_config` bit 0 = `1`), and `interval_code = 1`–`10`. Each decoded reading has the schema's `value_width`, unit, and scale. An `R` or `C` payload contains one or more Segments. Each Segment stores its first time and value as bases; each later reading stores a 1 B time gap from the previous reading and a signed 1 B value offset from the Segment's base value.

| Segment field | Size | Purpose |
|---|---:|---|
| **base_time** | 4 B | Time anchor for this Segment, using the schema's `time_format`. |
| **base_value** | 2, 4, 6, or 8 B | First reading in this Segment; `value_width` selects its size. |
| **offset_count** | 2 B | Number of later readings, each with a time offset and value offset; `0` means this Segment holds only its base reading. |
| **alignment_padding** | 0 or 2 B | `0xFF` bytes make the grouped fields above a multiple of 4 B; 2 B for 4 or 8 B bases. |
| **time_offset** | 1 B / later reading | Unsigned gap from the previous reading's time, in `interval_code` units. |
| **value_offset** | 1 B / later reading | Signed offset from this Segment's `base_value`. |

After the padding, each later reading is stored as `[time_offset][value_offset]`. The Segment's `base_time` gives the time of its base reading; add each unsigned `time_offset` to the previous reading's time to obtain the next time. `interval_value` records the nominal gap but is not used to decode segmented times. Start a new Segment when a time gap exceeds 255 units, time moves backward, or a value is outside the current base's offset range. The precise time encoding depends on the still-undefined `time_format`.

`value_offset` ranges from −127 to +127; the `-128` code is reserved. For temperature stored in tenths of a degree Fahrenheit, a 2 B `base_value` of 70.0 °F permits readings from 57.3 °F to 82.7 °F in that Segment. A new `C` starts when another Segment does not fit in the current block.

The block header's `payload_length` marks the end of the last Segment. Readers calculate the padding from `4 B base_time + value_width + 2 B offset_count`, verify and skip those `0xFF` bytes, then read exactly `offset_count` time/value pairs. The next Segment starts immediately after those pairs. Readers reject counts or padding that overrun `payload_length`; the final Segment ends exactly there. Padding is covered by the block CRC. The decoded reading count is the sum of `1 + offset_count` for all Segments. Writers buffer a Segment until its `offset_count` is known, then write it; the block CRC is finalized as usual.

## Total Storage Capacity

The table estimates segmented temperature storage (`codec = 0x10`) with 4 Segments and a 2 B `base_value` in each block. Each 4,084 B payload holds up to 2,030 readings: the 4 Segment headers use 32 B, and 4,052 B hold 2,026 time/value offset pairs when no alignment padding is needed. Readings arrive every 1, 10, 30, or 60 seconds, every 10 minutes, or every hour. Storage sizes use binary units. Reading counts are rounded to the nearest 1,000 (`K`); durations use the unrounded counts.

| Storage size | C blocks | Readings | Time to full<br>(1 sec) | Time to full<br>(10 secs) | Time to full<br>(30 secs) | Time to full<br>(1 min) | Time to full<br>(10 mins) | Time to full<br>(1 hr) |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 12K | 3.4 hrs | 1.4 days | 4.2 days | 8.5 days | 84.6 days | 1.4 yrs |
| 128 KiB | 29 | 61K | 16.9 hrs | 7.0 days | 21.1 days | 42.3 days | 1.2 yrs | 7.0 yrs |
| 1 MiB | 253 | 516K | 6.0 days | 59.7 days | 179.0 days | 358.1 days | 9.8 yrs | 58.9 yrs |
| 32 MiB | 8,189 | 16,626K | 0.5 yrs | 5.3 yrs | 15.8 yrs | 31.6 yrs | 316 yrs | 1,898 yrs |
| 126 MiB | 32,253 | 65,476K | 2.1 yrs | 20.8 yrs | 62.3 yrs | 125 yrs | 1,246 yrs | 7,474 yrs |
| 512 MiB | 131,069 | 266,072K | 8.4 yrs | 84.4 yrs | 253 yrs | 506 yrs | 5,062 yrs | 30,374 yrs |
| 1 GiB | 262,141 | 532,148K | 16.9 yrs | 169 yrs | 506 yrs | 1,012 yrs | 10,125 yrs | 60,748 yrs |

> Assumes 2 Superblocks and 1 `R` block, each occupying one sector; all remaining sectors hold `C` blocks. The estimate requires 4 Segments per block, 2 B per later reading, and no alignment padding. More Segments or padding reduce the reading count. Multiple datasets need additional `R` blocks.

## Recovery and Log Scanning

**Superblocks:**

- If only one copy validates, use it. If both validate but differ, `generation` identifies the newer fixed header. Increment it when updating the older copy; compare modulo 256.

**Schema log:**

- Use a schema entry only after both committed copies validate. If one copy is missing or invalid, append the valid entry to it first.
- If copies conflict for a `schema_id`, reject it and its records. New records may use the previous valid schema if compatible; a corrected schema needs a new ID.
- Recovery for partial or invalid entries, duplicate `schema_id`s, or entries in a different order between copies still needs definition.

**Append log and streams:**

- The first fully erased (`0xFF`) sector marks the append point. Skip non-erased sectors whose blocks have an invalid `magic_number` or `crc`; do not overwrite them.
- After restart, start a new `R` for each stream that resumes writing; do not resume an older `R` or `C`.
- While scanning in physical order, track the current extent for each `dataset_id`. Read its `interval_code`, `interval_value`, and `schema_id` from the `R` block.
- A valid Record block (`R`) starts an extent for its dataset. A valid Continuation block (`C`) extends that dataset's current extent; ignore a `C` when no valid extent is active.
- An invalid block ends the current extent for its dataset. Cleanup must preserve physical order within each dataset so Continuation blocks remain associated with the correct `R`.

**Cleanup:**

- Copy and validate live data before erasing old sectors. Preserve physical order within each dataset; define recovery from power loss during cleanup or compaction.

## Future Consideration

- Consider computing `crc` incrementally as block data is written, then storing the final CRC at block finalization. This processes the same bytes as a full-block calculation but spreads the work across writes; measure the call overhead on the target MCU.
