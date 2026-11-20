# SandglassDB

## Overview

The database starts with 2 Superblocks (`S0`, `S1`); all remaining blocks come from a shared storage pool. A Record block (`R`) starts an extent, and Continuation blocks (`C`) extend it when its payload fills. Each `dataset_id` identifies one stream; its `R` block specifies the `interval_code`. Blocks are ordered by their physical position in the log, so blocks from different streams may be interleaved.

A **Chest** is the set of column values for one reading, stored in an `R` or `C` payload. Live and archive blocks can each choose raw or uniformly quantized values, with optional lossless compression. Raw temperature values can use fixed-point integers; see [Archiving](README-ARCHIVE.md).

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


> In the field lists:
>
> - `*` means the field is expected to remain part of the format long term.
> - `**` means the field size is configurable.


## Superblocks (`S0`, `S1`)

The 2 fixed blocks used to configure the database are copies of each other. If one block is corrupted, for example, its **`magic number`** or **`CRC`** does not match, the other block can be used to recover the configuration.

The Superblock also needs a `version` number. It identifies the expected format and allows the format to change over time, such as from version 1 to version 2.

Each Superblock contains:

| field | size | purpose |
|---|---:|---|
| **config** | 1 byte* | Stores database-wide configuration options. |
| **version** | 1 byte* | Identifies the Superblock format. |
| **generation** | 1 byte* | Identifies the newer fixed Superblock header; wraps from `255` to `0`. |
| **magic_number** | 2 bytes* | Identifies the start of the Superblock header. |
| **flags** | 1 byte* | Stores Superblock flags. |
| **crc** | 4 bytes** | Covers the fixed header and alignment padding, excluding the CRC field itself and `schema_entries`. |
| **alignment_padding** | 2 bytes* | Set to `0`; aligns `schema_entries` to a 4-byte boundary. |
| **schema_entries** | 4,084 bytes | Append-only schema entries. Each entry has its own CRC and commit marker; see [Schema Entries](#schema-entries) for the entry layout. |

The Superblock is stored in 2 copies, `S0` and `S1`. Each copy has this field layout:
```text
       +--------+---------+------------+-------------+-------+-------+-------------------+---------------+
S0:    | config | version | generation | magic_number | flags | crc | alignment_padding | schema_entries |
       +--------+---------+------------+-------------+-------+-------+-------------------+---------------+
S1:    | config | version | generation | magic_number | flags | crc | alignment_padding | schema_entries |
       +--------+---------+------------+-------------+-------+-------+-------------------+---------------+
```

The fixed fields occupy 10 bytes. The 2-byte `alignment_padding` places `schema_entries` at byte offset 12; each schema entry is also padded to a 4-byte boundary.


## Schema Entries

Schema definitions are stored as append-only entries in the unused space of both Superblocks. Each schema update gets a new, unused `schema_id` and is appended after the previous entry in `S0` and `S1`; existing entries and IDs are not changed or reused. A record block may use the new `schema_id` only after the entry is committed and validated in both copies. This avoids erasing and rewriting the fixed Superblock fields for each schema change.

The application selects the `schema_id` for each new `R` block. Appending a schema entry does not switch streams automatically. For example, after adding `schema_id = 4`, the application switches a stream by writing its next `R` with `schema_id = 4`; that `R`'s `C` blocks use the same schema. Other streams can keep using their existing IDs.

To change a dataset's codec, append a new schema entry with the desired codec and start a new `R` using its `schema_id`. The new entry may keep the same column definitions. Existing `C` blocks continue using their parent `R`'s schema.

The CRC for the fixed Superblock fields does not cover the append-only entries; each entry has its own CRC and commit marker. Readers scan the entries in both copies and build a lookup from `schema_id` to payload layout.

Each schema entry contains schema-wide fields and per-column fields.

Schema-wide fields:

| Field | Size | Purpose |
|---|---:|---|
| **entry_crc** | 2 bytes* | Validates the schema entry. |
| **commit_marker** | 1 byte* | Marks the entry as fully written. |
| **schema_config1** | 1 byte* | Selects byte order and indicates optional per-column fields; see [schema_config1](#schema_config1). |
| **schema_id** | 1 byte* | Identifies the schema entry. |
| **col_count** | 1 byte* | Number of columns in the schema: `1`–`255` valid; `0` is reserved. |
| **codec** | 1 byte* | Selects the value representation and compression steps for every extent using this schema; see [Codec Byte](#codec-byte). |
| **time_format** | 1 byte* | Defines how the 4-byte `time_value` in each `R` using this schema is interpreted; format codes still need definition. |

Per-column fields, in entry order:

| Field | Size | Purpose |
|---|---:|---|
| **col_widths** | 1 byte / column | Allocates each column's space in a raw Chest in bytes. |
| **col_units** | 1 byte* / column | One unit code per column; see [`col_units` unit codes](#col_units-unit-codes). |
| **col_scale_mul** | 1 byte** / column | Present when `schema_config1` bit 4 is `1`; one 1-byte multiplier per column (default `1`). E.g., `10` with multiplier `3` becomes `30`. |
| **col_scale_exp** | 1 signed byte** / column | Present when `schema_config1` bit 2 is `1`; one base-10 exponent per column (default `0`). E.g., `123` with exponent `-2` becomes `1.23`. |
| **col_scale_ratios** | 2 bytes** / column | Present when `schema_config1` bit 3 is `1`; one numerator/denominator pair per column (default `1/1`). E.g., `10` with scale `3/2` becomes `15`. |
| **col_signs** | `ceil(col_count / 8)` bytes* | Used when `schema_config1` bit 0 is `1`; one bit per column (`1` = signed, `0` = unsigned). Unused bits are `0`. |
| **padding** | Variable | Aligns the entry to the flash program unit. |

### `schema_config1`

| Bit(s) | Meaning | Purpose |
|---|---|---|
| 7 | `reserved`: write `0`. | Reserved for future options. |
| 6 | `byte_order`: `0` = little-endian; `1` = big-endian. | Sets byte order for multi-byte column values. |
| 5 | `reserved`: write `0`. | Reserved for future options. |
| 4 | `col_scale_mul`: `1` = yes; `0` = no (multiplier defaults to `1`). | Indicates whether `col_scale_mul` values follow. |
| 3 | `col_scale_ratios`: `1` = yes; `0` = no (scale defaults to `1/1`). | Indicates whether `col_scale_ratios` follows. |
| 2 | `col_scale_exp`: `1` = yes; `0` = no (exponent defaults to `0`). | Indicates whether `col_scale_exp` follows. |
| 1 | `reserved`: write `0`. | Reserved for future options. |
| 0 | `col_signs`: `1` = use; `0` = ignore (all columns unsigned). | Controls whether readers apply the always-stored `col_signs` bits. |

### Codec Byte

Each schema entry stores a 1-byte `codec`. An `R` selects the schema for its extent, and its `C` blocks inherit that codec. The proposed bits separate the stored value representation from the lossless compression steps:

| Bit(s) | Meaning | Purpose |
|---:|---|---|
| 7–4 | Reserved; write `0`. | Leaves room for future encodings. |
| 3 | `rle`: `0` = off; `1` = on. | Run-length encodes repeated results after prediction. |
| 2–1 | `predictor`: `00` = none; `01` = delta; `10` = delta-of-delta; `11` = reserved. | Encodes changes between consecutive values of each column. |
| 0 | `representation`: `0` = raw; `1` = uniform quantization. | Selects the values passed to the predictor. |

For example, `0x00` means raw, `0x01` means uniform quantization, `0x03` means quantization plus delta, and `0x0B` adds RLE to that combination. A reader rejects reserved or unsupported combinations.

Encode in this order: raw values or uniform quantization → predictor → RLE → payload. Decode in reverse order. Delta starts with one unpredicted value per column; delta-of-delta starts with two. Predictor state resets at each block boundary, so every block decodes independently. RLE operates on the predictor results, and `chest_count` is the number of Chests after decoding. Delta and RLE preserve their input exactly; uniform quantization may introduce rounding error.

Uniform quantization needs a range and bit width for each quantized column in the schema. The precise metadata fields, bit packing, signed residual encoding, RLE run format, and out-of-range rule still need definition. Until those rules are specified, write only `codec = 0x00`; readers reject other values. See [Archiving](README-ARCHIVE.md) for how to compare the combinations.

### `col_units` Unit Codes

Unit codes are 1 byte and are assigned as follows:

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

When reading, the schema log answers: “Given a `schema_id`, what is the byte layout of the payload?” For example:

```text
payload     = 0x01 0x02 0x03 0x04
temperature = 0x01 0x02
humidity    = 0x03 0x04
```

A three-column schema can be represented as:

```text
schema_id     = 0
col_count  = 3
codec = 0x00                 # raw values
schema_config1   = 0b00000001  # col_signs is enabled
col_widths = [1, 2, 4]        # bytes per column
col_signs  = 0b01000000
col_units  = [0, 0, 0]        # unspecified units
```

This describes an unsigned 8-bit column, a signed 16-bit column, and an unsigned 32-bit column. Each schema change appends a new entry to both Superblocks. Existing extents retain their original `schema_id`; new data using a new schema starts in a new record block.

### Schema Entry Capacity

Each Superblock has 4,084 bytes for append-only `schema_entries`. This estimate assumes 4-byte entry alignment, `col_signs` and `col_scale_ratios` present, and other optional arrays absent. The 8 fixed bytes come from the schema-wide fields. For every column, `col_widths`, `col_units`, and `col_scale_ratios` add 4 bytes total.

| `col_count` | Aligned entry size | Entries per Superblock | Unused bytes per Superblock |
|---:|---:|---:|---:|
| 4 | 28 bytes | 145 | 24 |
| 8 | 44 bytes | 92 | 36 |
| 16 | 76 bytes | 53 | 56 |
| 32 | 140 bytes | 29 | 24 |
| 64 | 272 bytes | 15 | 4 |

The entry count includes the initial schema, so the maximum number of schema updates is `entry count − 1`. Both Superblocks store the same entries; they are copies, not extra capacity.

## Streams

Each `dataset_id` identifies one stream. The `R` header stores its `interval_code`; its `C` blocks store `dataset_id` and inherit the interval from the current `R`. This permits up to 256 dataset IDs.

The application can change a dataset's interval by starting a new `R`, which ends the previous extent for that dataset. Following `C` blocks for the dataset belong to the new `R`. Use separate dataset IDs to write multiple intervals at the same time. See [Interval Codes](#interval-codes).

The library appends all streams to one physical log, so their blocks can interleave. Readers scan the log in physical order and track the current extent for each `dataset_id`.

```text
Physical append order (each row is one 4 KiB block):
[R header: dataset_id = 0, interval_code = 1] [payload]
[R header: dataset_id = 1, interval_code = 2] [payload]
[C header: dataset_id = 0] [payload]
[C header: dataset_id = 1] [payload]
[C header: dataset_id = 0] [payload]

Reader separates the streams:
dataset_id = 0, interval_code = 1: [R] → [C] → [C]
dataset_id = 1, interval_code = 2: [R] → [C]
```

`schema_id` is independent of `dataset_id`. Each `R` selects the schema for its extent, and its `C` blocks use that schema. Different streams can use different schemas; a stream can switch schemas by starting a new `R` extent.

## Record blocks (`R`)

Record blocks (`R0`–`Rn`) follow Superblocks `S0` and `S1`. Each record block belongs to the stream identified by its `dataset_id` and uses a `schema_id` whose entry is committed in both Superblocks. Different streams can allocate blocks from the same shared pool.

A record block contains:

| field | size | purpose |
|---|---:|---|
| **crc** | 2 bytes** | Covers the block header and payload, including `chest_count`; excludes itself. |
| **magic_number** | 2 bytes* | Identifies the start of the record block. |
| **time_value** | 4 bytes** | Stores the block's time value in the format specified by its schema. |
| **chest_count** | 2 bytes* | Number of Chests in this block; written at finalization to locate the payload areas. |
| **dataset_id** | 1 byte* | Identifies the dataset. |
| **schema_id** | 1 byte* | Identifies the schema defining this block's Chest layout. |
| **interval_code** | 1 byte* | Selects the stream's timestamp interval. |
| **payload** | Variable | Stores Chests using the selected codec; see [Payload](#payload). |

The Record block header is 13 bytes. At finalization, write `chest_count`, then compute and store `crc`; readers use the count only if the CRC validates. The `schema_id` selects this extent's codec and `time_format`; its `C` blocks use the same schema and codec.

### Interval Codes

Each `R` stores the `interval_code` for its extent; its `C` blocks inherit it. A new `R` may select a different interval for the same `dataset_id`.

| `interval_code` | Interval |
|---:|---|
| 36–255 | Reserved |
| 31–35 | 31 = 1 year; 32 = 2 years; 33 = 3 years; 34 = 4 years; 35 = 5 years |
| 28–30 | 28 = 4 months; 29 = 5 months; 30 = 6 months |
| 25–27 | 25 = 1 month; 26 = 2 months; 27 = 3 months |
| 22–24 | 22 = 1 week; 23 = 2 weeks; 24 = 3 weeks |
| 19–21 | 19 = 4 days; 20 = 5 days; 21 = 6 days |
| 16–18 | 16 = 1 day; 17 = 2 days; 18 = 3 days |
| 12–15 | 12 = 15 minutes; 13 = 20 minutes; 14 = 30 minutes; 15 = 60 minutes |
| 8–11 | 8 = 1 minute; 9 = 2 minutes; 10 = 5 minutes; 11 = 10 minutes |
| 4–7\* | 4 = 10 seconds; 5 = 15 seconds; 6 = 20 seconds; 7 = 30 seconds |
| 1–3\* | 1 = 1 second; 2 = 2 seconds; 3 = 5 seconds |
| 0\* | Timestamp stored in each Chest |

> \* These codes are for records written live. Archiving processes all records in a completed source block to produce records for longer intervals.

For `interval_code = 0`, the timestamp column and its format must be defined for the Chest; how the schema marks that column remains to be specified. For codes `1`–`15`, the reader derives timestamps from the extent's start time and each Chest's position. Calendar boundaries, time zone, and week start for codes `16`–`35` still need definition.

## Continuation blocks (`C`)

Continuation blocks store records that follow a record block `R` when its payload is full. Each carries the same `dataset_id` and is associated with the current extent for that dataset by physical log order, even when other streams' blocks are interleaved. It inherits `interval_code` and `schema_id` from the preceding `R` for that dataset. When a continuation block `C` fills, another continuation block is added. See [Recovery and Log Scanning](#recovery-and-log-scanning) for the ordering rules.

A continuation block contains:

| Field | Size | Purpose |
|---|---:|---|
| **crc** | 2 bytes** | Covers the block header and payload, including `chest_count`; excludes itself. |
| **magic_number** | 2 bytes* | Identifies the start of the continuation block. |
| **time_value** | 4 bytes** | Stores the time of the first Chest in this block, using the format specified by its schema. |
| **chest_count** | 2 bytes* | Number of Chests in this block; written at finalization to locate the payload areas. |
| **dataset_id** | 1 byte* | Identifies the dataset. |
| **payload** | Variable | Stores more Chests using the schema and codec of this extent's Record block. |

The Continuation block uses the codec and time format in its parent `R`'s schema entry. Its header is 11 bytes. `time_value` marks the first Chest in the block; for `interval_code = 0`, each Chest still stores its own timestamp. At finalization, write `chest_count`, then compute and store `crc`; readers use the count only if the CRC validates.

Approximate records per block, assuming each chest is entirely contained in 1 block:

Continuation block header: 11 bytes.

| Chest size | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,020 | 3 |
| 8 bytes | 510 | 3 |
| 12 bytes | 340 | 3 |
| 16 bytes | 255 | 3 |
| 20 bytes | 204 | 3 |
| 24 bytes | 170 | 3 |

> Unused bytes are the remaining payload space after storing whole records. The counts assume records are packed consecutively with no alignment padding.
## Payload

With codec `0`, each Record or Continuation block stores raw Chests consecutively from the start of its payload:

```text
Extent example (each row is one 4 KiB block):
[R header] [Chest 0] [Chest 1] [Chest 2] ...
[C header] [Chest 0] [Chest 1] [Chest 2] ...
[C header] [Chest 0] [Chest 1] [Chest 2] ...
```

The schema's `col_widths` determines each raw Chest's column sizes in bytes. At finalization, write the 2-byte `chest_count` in the header, then compute and store `crc` over the header and payload, excluding the CRC field. Readers use the validated count and schema to find the end of the raw Chests. Any remaining payload bytes are unused. A future compressed codec must define its own byte layout and how the count maps to decoded Chests.

## Total Storage Capacity

The table assumes one raw 4-byte Chest every 1, 10, or 30 seconds. Counts include the capacity of 1 Record block and all Continuation blocks. Storage sizes use binary units.

| Storage size | Continuation<br>blocks | Continuation<br>header space | Records | Time to full<br>(1 sec) | Time to full<br>(10 secs) | Time to full<br>(30 secs) |
|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 0.05 KiB | 6,120 | ~1.7 hours | ~17.0 hours | ~2.1 days |
| 128 KiB | 29 | 0.3 KiB | 30,600 | ~8.5 hours | ~3.5 days | ~10.6 days |
| 1 MiB | 253 | 2.7 KiB | 259,080 | ~3.0 days | ~30.0 days | ~90.0 days |
| 32 MiB | 8,189 | 88 KiB | 8.4 × 10⁶ | ~96.7 days | ~2.6 years | ~7.9 years |
| 126 MiB | 32,253 | 347 KiB | 32.9 × 10⁶ | ~1.0 years | ~10.4 years | ~31.3 years |
| 512 MiB | 131,069 | 1,408 KiB | 133.7 × 10⁶ | ~4.2 years | ~42.4 years | ~127.2 years |
| 1 GiB | 262,141 | 2,816 KiB | 267.4 × 10⁶ | ~8.5 years | ~84.8 years | ~254.4 years |

> Assumes 2 Superblocks and 1 Record block; all remaining 4 KiB blocks are Continuation blocks. With codec `0`, the Record block and each Continuation block hold 1,020 Chests. Multiple streams need additional Record blocks.

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
- While scanning in physical order, track the current extent for each `dataset_id`. Read its `interval_code` and `schema_id` from the `R` block.
- A valid Record block (`R`) starts an extent for its dataset. A valid Continuation block (`C`) extends that dataset's current extent; ignore a `C` when no valid extent is active.
- An invalid block ends the current extent for its dataset. Cleanup must preserve physical order within each dataset so Continuation blocks remain associated with the correct `R`.

**Cleanup:**

- Copy and validate live data before erasing old blocks. Preserve physical order within each dataset; define recovery from power loss during cleanup or compaction.

## Future Consideration

- Consider computing `crc` incrementally as block data is written, then storing the final CRC at block finalization. This processes the same bytes as a full-block calculation but spreads the work across writes; measure the call overhead on the target MCU.
