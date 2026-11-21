# SandglassDB

## Overview

The database starts with 2 Superblocks (`S0`, `S1`); all remaining blocks come from a shared storage pool. A Record block (`R`) starts an extent, and Continuation blocks (`C`) extend it when its payload fills. Each `dataset_id` identifies one stream; its `R` block specifies the `interval_code` and `interval_value`. Blocks from different datasets may interleave. Each `C` belongs to the most recent preceding `R` with the same `dataset_id`.

Each dataset stores one measurement type, such as temperature or humidity. One reading is one value in an `R` or `C` payload. The extent's schema defines that value's width, signedness, unit, scale, and codec. Live and archive datasets can use different schemas; see [Archiving](README-ARCHIVE.md) for an encoding example.

```text
[S0] [S1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Each bracket represents one 4 KiB block.


Flash storage is divided into blocks. Each block can store 4094 bytes of data.

When a database is accessed, it needs at least two things:

- **Magic number:** Identifies the start of a header and indicates that the information for that header begins at this address.
- **CRC checksum:** Detects accidental changes or corruption in raw data. A mathematical operation reduces each set of data—for example, 250 bytes to a smaller checksum. Compare the checksums for 2 sets to check whether the data is the same, **`without comparing all 250 bytes one by one`**:

  ```text
  Set 1: 250 bytes of data → 2-byte checksum 1
  Set 2: 250 bytes of data → 2-byte checksum 2
  ```

  If the checksums match, the data is likely the same; if they differ, the data is different or corrupted.

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
| **value_config** | 1 B | Sets signedness, byte order, and optional scale fields; see [value_config](#value_config). |
| **schema_id** | 1 B | Identifies the schema entry. |
| **codec** | 1 B | Selects how this value is stored; see [Codec Byte](#codec-byte). |
| **time_format** | 1 B | Defines the 4 B `time_value` in `R` and `C`; format codes still need definition. |
| **value_width** | 1 B | Number of bytes in a decoded raw value (`1`–`8`). |
| **value_unit** | 1 B | Unit code for the value; see [Unit Codes](#unit-codes). |
| **value_scale_mul** | 1 B | Present when `value_config` bit 4 is `1`; multiplier (default `1`). |
| **value_scale_exp** | 1 signed B | Present when `value_config` bit 2 is `1`; base-10 exponent (default `0`). |
| **value_scale_ratio** | 2 B | Present when `value_config` bit 3 is `1`; numerator and denominator (default `1/1`). |
| **padding** | Variable | Aligns the entry to the flash program unit. |

Signed values use two's complement. The decoded value is the stored integer multiplied by `value_scale_mul`, `10^value_scale_exp`, and the `value_scale_ratio` numerator divided by its denominator. The denominator must be nonzero. For example, `123` with exponent `-2` represents `1.23`; `10` with ratio `3/2` represents `15`.

### `value_config`

| Bit(s) | Meaning | Purpose |
|---|---|---|
| 7 | Reserved; write `0`. | Reserved for future options. |
| 6 | `byte_order`: `0` = little-endian; `1` = big-endian. | Sets byte order for multi-byte values. |
| 5 | Reserved; write `0`. | Reserved for future options. |
| 4 | `value_scale_mul`: `1` = present; `0` = default `1`. | Enables the multiplier field. |
| 3 | `value_scale_ratio`: `1` = present; `0` = default `1/1`. | Enables the ratio field. |
| 2 | `value_scale_exp`: `1` = present; `0` = default `0`. | Enables the exponent field. |
| 1 | Reserved; write `0`. | Reserved for future options. |
| 0 | `signed`: `1` = signed; `0` = unsigned. | Selects how the value is interpreted. |

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

Each Superblock has 4,084 B for append-only `schema_entries`. Entries are padded to 4 B. The 11 fixed bytes are `entry_crc`, `commit`, `value_config`, `schema_id`, `codec`, `time_format`, `value_width`, and `value_unit`.

| Scale fields | Aligned entry size | Entries per Superblock | Unused space per Superblock |
|---|---:|---:|---:|
| None | 12 B | 256 | 1,012 B |
| Multiplier, exponent, and ratio | 16 B | 255 | 4 B |

The 1 B `schema_id` limits the table to 256 distinct entries. The entry count includes the initial schema; both Superblocks hold the same entries.

## Streams

Each `dataset_id` identifies one stream. The `R` header stores its `interval_code` and `interval_value`; its `C` blocks store `dataset_id` and inherit the interval from the current `R`. This permits up to 256 dataset IDs.

The application can change a dataset's interval by starting a new `R`, which ends the previous extent for that dataset. Following `C` blocks for the dataset belong to the new `R`. Use separate dataset IDs to write multiple intervals at the same time. See [Interval Codes](#interval-codes).

The library appends all streams to one physical log, so their blocks can interleave. Readers scan the log in physical order and track the current extent for each `dataset_id`.

```text
Physical append order (each row is one 4 KiB block):
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

Record blocks (`R0`–`Rn`) follow Superblocks `S0` and `S1`. Each record block belongs to the stream identified by its `dataset_id` and uses a `schema_id` whose entry is committed in both Superblocks. Different streams can allocate blocks from the same shared pool.

A record block contains:

| field | size | purpose |
|---|---:|---|
| **crc** | 4 B | Covers the header, including `payload_length`, and the used payload; excludes itself. |
| **magic_number** | 2 B | Identifies the start of the record block. |
| **payload_length** | 2 B | Number of valid payload bytes in this block (`0`–`4,078`). |
| **time_value** | 4 B | Stores the time of the first reading in this block, using the format specified by its schema. |
| **dataset_id** | 1 B | Identifies the dataset. |
| **schema_id** | 1 B | Identifies the schema defining this block's value format. |
| **interval_code** | 1 B | Selects the time unit. |
| **interval_value** | 1 B | Sets the number of units between readings. |
| **payload** | 4,078 B remaining | Stores raw readings or encoded segments according to the schema's codec; see [Payload Formats](#payload-formats). |

The Record block header is 16 bytes. At finalization, write `payload_length`, then compute and store `crc`; readers use the length only if the CRC validates. The `schema_id` selects this extent's codec and `time_format`; its `C` blocks use the same schema and codec.

### Interval Codes

Each `R` stores an `interval_code` and `interval_value` for its extent; its `C` blocks inherit both. For codes `1`–`10`, `interval_value` ranges from `1` to `255`. For code `0`, write `interval_value = 0` because each reading carries its own timestamp. For example, code `5` with value `10` means one reading every 10 minutes.

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

For `interval_code = 0`, store a 4 B timestamp before each raw value, using the schema's `time_format`; the header's `time_value` matches the first timestamp. For codes `1`–`6`, the reader derives timestamps from each block's `time_value`, `interval_value`, and the reading's position within that block. Calendar boundaries, time zone, and week start for codes `7`–`10` still need definition.

## Continuation blocks (`C`)

Continuation blocks store records that follow a record block `R` when its payload is full. Each carries the same `dataset_id` and is associated with the current extent for that dataset by physical log order, even when other streams' blocks are interleaved. It inherits `interval_code`, `interval_value`, and `schema_id` from the preceding `R` for that dataset. When a continuation block `C` fills, another continuation block is added. See [Recovery and Log Scanning](#recovery-and-log-scanning) for the ordering rules.

A continuation block contains:

| Field | Size | Purpose |
|---|---:|---|
| **crc** | 4 B | Covers the header, including `payload_length`, and the used payload; excludes itself. |
| **magic_number** | 2 B | Identifies the start of the continuation block. |
| **payload_length** | 2 B | Number of valid payload bytes in this block (`0`–`4,078`). |
| **time_value** | 4 B | Stores the measured time of the first reading in this block, using the format specified by its schema. |
| **dataset_id** | 1 B | Identifies the dataset. |
| **alignment_padding** | 3 B | Set to `0`; aligns the payload to a 4-byte boundary. |
| **payload** | 4,078 B remaining | Stores raw readings or encoded segments using the schema of this extent's Record block. |

The Continuation block uses its own `time_value` and inherits `interval_code`, `interval_value`, time format, schema, and codec from its parent `R`. Its header is 16 B. Each block's `time_value` anchors its first reading, so timing drift does not accumulate across blocks. For `interval_code = 0`, each reading stores its own timestamp. At finalization, write `payload_length`, then compute and store `crc`; readers use the length only if the CRC validates.

Raw fixed-interval readings per `C` block, assuming each value is entirely contained in 1 block and the header is 16 B:

| Value width | Readings per block | Unused payload (B) |
|---:|---:|---:|
| 1 B | 4,078 | 0 |
| 2 B | 2,039 | 0 |
| 4 B | 1,019 | 2 |
| 8 B | 509 | 6 |

> Unused bytes are the remaining payload space after storing whole readings. The counts assume readings are packed consecutively with no alignment padding. For `interval_code = 0`, add 4 B per reading for its timestamp.

## Payload Formats

Before checking the CRC, readers reject a `payload_length` greater than `4,078` B. Only the first `payload_length` bytes of the payload are valid and covered by the CRC.

### Raw Readings (`codec = 0x00`)

With codec `0`, each `R` or `C` stores raw readings consecutively from the start of its payload:

```text
Extent example (each row is one 4 KiB block):
[R header] [reading 0] [reading 1] [reading 2] ...
[C header] [reading 0] [reading 1] [reading 2] ...
[C header] [reading 0] [reading 1] [reading 2] ...
```

For `interval_code = 1`–`10`, each reading is one `value_width`-byte value. For `interval_code = 0`, each reading is a 4 B timestamp followed by one `value_width`-byte value. At finalization, write the 2 B `payload_length` in the header, then compute and store `crc` over the header and that many payload bytes, excluding the CRC field. The validated length must be a multiple of the reading size (`value_width`, or `4 + value_width` for code `0`); the quotient is the reading count. Remaining payload bytes are unused. Other codecs must define their own byte layouts and decoded reading counts.

### Segmented Base and Offsets (`codec = 0x10`)

This codec requires `value_width = 2` or `4`, signed values (`value_config` bit 0 = `1`), and `interval_code = 1`–`10`. Each decoded reading has the schema's `value_width`, unit, and scale. Both `R` and `C` retain their existing headers. Each block stores one or more segments consecutively in its payload, with fields in this order:

| Part | Field | Size | Purpose |
|---|---|---:|---|
| Property | **reading_count** | 2 B | Number of readings in the segment, including `base_value`; at least `1`. |
| Property | **base_value** | 2 B or 4 B | First reading in the segment; `value_width` selects its size, and all offsets are relative to it. |
| Payload | **offsets** | `reading_count − 1` B | One signed 1 B offset per later reading; each is added to `base_value`. |

Offsets range from −127 to +127; the `-128` code is reserved. For temperature stored in tenths of a degree Fahrenheit, a 2 B base of 70.0 °F permits readings from 57.3 °F to 82.7 °F in that segment. Start a new segment when the next reading falls outside the offset range. A new `C` starts when another segment or offset does not fit in the current block; its first segment has a new base.

The block header's `payload_length` marks the end of the last segment. Readers parse segments until they consume exactly that many bytes, rejecting a count that would overrun the used payload. The decoded reading count is the sum of the segment `reading_count` values. The block's `time_value` anchors its first reading; positions continue across segments within the block. The writer must know a segment's count before programming its 2 B count field, so it buffers the current segment and writes it once when the segment closes. The existing block CRC is finalized as usual.

## Total Storage Capacity

The table estimates segmented temperature storage (`codec = 0x10`) with a 2 B base and exactly 4 segments per block. Each 4,078 B payload holds up to 4,066 readings: 4 segments use 16 B for their bases and counts, and the remaining bytes hold 4,062 signed 1 B offsets. Readings arrive every 1, 10, or 30 seconds (`interval_code = 4`). Storage sizes use binary units.

| Storage size | Continuation<br>blocks | Continuation<br>header space | Readings | Time to full<br>(1 sec) | Time to full<br>(10 secs) | Time to full<br>(30 secs) |
|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 0.08 KiB | 24,396 | ~6.8 hours | ~2.8 days | ~8.5 days |
| 128 KiB | 29 | 0.5 KiB | 121,980 | ~1.4 days | ~14.1 days | ~42.4 days |
| 1 MiB | 253 | 4.0 KiB | 1,032,764 | ~12.0 days | ~119.5 days | ~358.6 days |
| 32 MiB | 8,189 | 128 KiB | 33,300,540 | ~1.1 years | ~10.6 years | ~31.7 years |
| 126 MiB | 32,253 | 504 KiB | 131,144,764 | ~4.2 years | ~41.6 years | ~124.7 years |
| 512 MiB | 131,069 | 2,048 KiB | 532,930,620 | ~16.9 years | ~168.9 years | ~506.6 years |
| 1 GiB | 262,141 | 4,096 KiB | 1,065,869,372 | ~33.8 years | ~337.8 years | ~1,013.3 years |

> Assumes 2 Superblocks and 1 `R` block; all remaining 4 KiB blocks are `C` blocks. The estimate requires every reading to fit within one of the 4 segments in its block. Multiple datasets need additional `R` blocks.

## Recovery and Log Scanning

**Superblocks:**

- If only one copy validates, use it. If both validate but differ, `generation` identifies the newer fixed header. Increment it when updating the older copy; compare modulo 256.

**Schema log:**

- Use a schema entry only after both committed copies validate. If one copy is missing or invalid, append the valid entry to it first.
- If copies conflict for a `schema_id`, reject it and its records. New records may use the previous valid schema if compatible; a corrected schema needs a new ID.
- Recovery for partial or invalid entries, duplicate `schema_id`s, or entries in a different order between copies still needs definition.

**Append log and streams:**

- The first fully erased (`0xFF`) block marks the append point. Skip non-erased blocks with invalid `magic_number` or `crc`; do not overwrite them.
- After restart, start a new `R` for each stream that resumes writing; do not resume an older `R` or `C`.
- While scanning in physical order, track the current extent for each `dataset_id`. Read its `interval_code`, `interval_value`, and `schema_id` from the `R` block.
- A valid Record block (`R`) starts an extent for its dataset. A valid Continuation block (`C`) extends that dataset's current extent; ignore a `C` when no valid extent is active.
- An invalid block ends the current extent for its dataset. Cleanup must preserve physical order within each dataset so Continuation blocks remain associated with the correct `R`.

**Cleanup:**

- Copy and validate live data before erasing old blocks. Preserve physical order within each dataset; define recovery from power loss during cleanup or compaction.

## Future Consideration

- Consider computing `crc` incrementally as block data is written, then storing the final CRC at block finalization. This processes the same bytes as a full-block calculation but spreads the work across writes; measure the call overhead on the target MCU.
