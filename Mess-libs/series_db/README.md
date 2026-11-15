# SandglassDB

## Overview

The database starts with 2 Superblocks (`S0`, `S1`); all remaining blocks come from a shared storage pool. A Record block (`R`) starts an extent, and Continuation blocks (`C`) extend it when its payload fills. Each data block belongs to a resolution stream; `stream_id` identifies the stream and `stream_sequence` orders its blocks. Streams can share the pool, so their blocks may be interleaved.

A **Chest** is the set of column values for one record, stored in the readings area of an `R` or `C` payload. A **`Rune`** is a 1-byte coefficient assigned to a bit position. An **`Essence`** is a packed bit mask stored in the block payload for a column and reading. Like a quantization code, each bit controls one Rune: `1` adds its Rune to the value, and `0` does not. For Runes `[a, b]`, Essence `10` produces `b`.

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
| **magic_number** | 2 bytes* | Identifies the start of the Superblock header. |
| **crc16** | 4 bytes** | Covers `config`, `version`, `magic_number`, and `flags`, excluding the CRC field itself and `schema_entries`. |
| **flags** | 1 byte* | Stores Superblock flags. |
| **schema_entries** | Remaining space | Append-only schema entries. Each entry has its own CRC and commit marker; see [Schema Entries](#schema-entries) for the entry layout. |

The Superblock is stored in 2 copies, `S0` and `S1`. Each copy has this field layout:
```text
       +--------+---------+-------------+-------+-------+---------------+
S0:    | config | version | magic_number | flags | crc16 | schema_entries |
       +--------+---------+-------------+-------+-------+---------------+
S1:    | config | version | magic_number | flags | crc16 | schema_entries |
       +--------+---------+-------------+-------+-------+---------------+
```


## Schema Entries

Schema definitions are stored as append-only entries in the unused space of both Superblocks. Each new schema is appended after the previous entry in `S0` and `S1`. A record block may use the new `schema_id` only after the entry is committed and validated in both copies. This avoids erasing and rewriting the fixed Superblock fields for each schema change.

The CRC for the fixed Superblock fields does not cover the append-only entries; each entry has its own CRC and commit marker. Readers scan the entries in both copies and build a lookup from `schema_id` to payload layout.

Each schema entry contains schema-wide fields and per-column fields.

Schema-wide fields:

| Field | Size | Purpose |
|---|---:|---|
| **entry_crc** | 2 bytes* | Validates the schema entry. |
| **commit_marker** | 1 byte* | Marks the entry as fully written. |
| **schema_config1** | 1 byte* | Selects byte order and indicates optional per-column fields; see [schema_config1](#schema_config1). |
| **schema_id** | 1 byte* | Identifies the schema entry. |
| **col_count** | 1 byte* | Number of columns in the schema: `1`–`16` valid; `0` and `17`–`255` reserved. |
| **codec** | 1 byte* | Selects how blocks using this schema encode their payload; IDs use the low 3 bits, and the high 5 bits are `0`. See [Codec IDs](#codec-ids). |

Per-column fields, in entry order:

| Field | Size | Purpose |
|---|---:|---|
| **col_widths** | 1 byte / column | Allocates each column's space in the readings area in bytes. It does not store quantization settings. |
| **col_units** | 1 byte* / column | One unit code per column; see [`col_units` unit codes](#col_units-unit-codes). |
| **col_scale_mul** | 1 byte** / column | Present when `schema_config1` bit 4 is `1`; one 1-byte multiplier per column (default `1`). E.g., `10` with multiplier `3` becomes `30`. |
| **col_scale_exp** | 1 signed byte** / column | Present when `schema_config1` bit 2 is `1`; one base-10 exponent per column (default `0`). E.g., `123` with exponent `-2` becomes `1.23`. |
| **col_scale_ratios** | 2 bytes** / column | Present when `schema_config1` bit 3 is `1`; one numerator/denominator pair per column (default `1/1`). E.g., `10` with scale `3/2` becomes `15`. |
| **col_signs** | `ceil(col_count / 8)` bytes* | Used when `schema_config1` bit 0 is `1`; one bit per column (`1` = signed, `0` = unsigned). Unused bits are `0`. |
| **col_essence_bits** | 1 byte** / column | Present when `schema_config1` bit 1 is `1`; sets each column's Essence width (`0` = no Essence; `1`, `2`, or `4` bits per reading). Other values are reserved. |
| **col_runes** | 1 byte × total Essence bits** | Present when `schema_config1` bit 1 is `1`; Rune coefficients ordered by column, then bit position. |
| **padding** | Variable | Aligns the entry to the flash program unit. |

Total Essence bits is the sum of `col_essence_bits` across all columns. For example, 3 columns with 2-bit Essences need 6 Runes, occupying 6 bytes. Runes are stored once in the Schema Entry; each reading's Essence selects which Runes to add.


### `schema_config1`

| Bit(s) | Meaning | Purpose |
|---|---|---|
| 7 | `reserved`: write `0`. | Reserved for future options. |
| 6 | `byte_order`: `0` = little-endian; `1` = big-endian. | Sets byte order for multi-byte column values. |
| 5 | `reserved`: write `0`. | Reserved for future options. |
| 4 | `col_scale_mul`: `1` = yes; `0` = no (multiplier defaults to `1`). | Indicates whether `col_scale_mul` values follow. |
| 3 | `col_scale_ratios`: `1` = yes; `0` = no (scale defaults to `1/1`). | Indicates whether `col_scale_ratios` follows. |
| 2 | `col_scale_exp`: `1` = yes; `0` = no (exponent defaults to `0`). | Indicates whether `col_scale_exp` follows. |
| 1 | `col_essence_bits`: `1` = yes; `0` = no. | Indicates whether `col_essence_bits` and `col_runes` follow. |
| 0 | `col_signs`: `1` = use; `0` = ignore (all columns unsigned). | Controls whether readers apply the always-stored `col_signs` bits. |

### Codec IDs

| ID | Codec | Meaning |
|---:|---|---|
| `7`–`3` | Reserved | Do not write; readers should reject blocks using these IDs as unsupported. |
| `2` | Delta + varint | Store the first record raw, then signed per-field differences from the previous record as variable-length integers. The exact integer encoding and field ordering must be specified before implementation. |
| `1` | Run-length encoding (RLE) | Store repeated identical records once with a count. |
| `0` | Raw | Store records unchanged according to the schema; default. |

The codec applies to every Record and Continuation block using this schema. Until non-raw decoding and record-boundary rules are implemented, write codec ID `0`.

### `col_units` Unit Codes

Unit codes are 1 byte and are assigned as follows:

| Codes | Units, in code order |
|---:|---|
| `0–1` | Unspecified, Count |
| `2` | Thaums |
| `3–4` | Percent (%), Parts per million (ppm) |
| `5–9` | Lux, Decibel milliwatt (dBm), Decibel (dB), Degree (°), Radian |
| `10–12` | Kelvin (K), Celsius (°C), Fahrenheit (°F) |
| `13–16` | Inch, Foot, Yard, Mile |
| `17–19` | Ounce, Pound, Short ton (US) |
| `20–22` | Nanosecond, Microsecond, Millisecond |
| `23–29` | Second, Minute, Hour, Day, Calendar month, Calendar year, Week |
| `30–32` | Hertz (Hz), kilohertz (kHz), gigahertz (GHz) |

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
schema_config1   = 0b00000001  # col_signs is enabled
col_widths = [1, 2, 4]        # bytes per column
col_signs  = 0b01000000
col_units  = [0, 0, 0]        # unspecified units
```

This describes an unsigned 8-bit column, a signed 16-bit column, and an unsigned 32-bit column. Each schema change appends a new entry to both Superblocks. Existing extents retain their original `schema_id`; new data using a new schema starts in a new record block.

## Record blocks (`R`)

Record blocks (`R0`–`Rn`) follow Superblocks `S0` and `S1`. Each record block belongs to a logical resolution stream, identified by `stream_id`, and uses a `schema_id` whose entry is committed in both Superblocks. Different streams can allocate blocks from the same shared pool.

A record block contains:

| field | size | purpose |
|---|---:|---|
| **magic_number** | 2 bytes* | Identifies the start of the record block. |
| **crc16** | 2 bytes** | Detects accidental changes or corruption in the block. |
| **schema_id** | 1 byte* | Identifies the schema containing this block's `col_widths` and optional `col_essence_bits`. |
| **stream_id** | 1 byte* | Identifies the resolution stream this block belongs to. |
| **stream_sequence** | 4 bytes* | Orders this block within its stream. |
| **record_config** | 5 bytes** | Stores the block time and time format. |
| **payload** | Variable | Stores readings and, when enabled, a packed Essence area; see [Payload](#payload). |

`col_essence_bits` is stored once in the schema entry and selects the Essence width for each column. The Essences are stored in the block payload.

Approximate records per block, assuming each chest is entirely contained in 1 block. A **chest** contains all column values for one record; its size excludes Essences:

Record block header: 15 bytes.

| Chest size | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,019 | 3 |
| 8 bytes | 509 | 7 |
| 12 bytes | 339 | 11 |
| 16 bytes | 254 | 15 |
| 20 bytes | 203 | 19 |
| 24 bytes | 169 | 23 |


## Record Configuration

`record_config` occupies 5 bytes:

| field | size | purpose |
|---|---:|---|
| `time_value` | 4 bytes | Stores the time value for records in the block. |
| `time_format` | 1 byte | Identifies how the time value is represented. |

## Continuation blocks (`C`)

Continuation blocks store records that follow a record block `R` when its payload is full. Each belongs to the same `stream_id` and carries a per-stream block sequence so it can be associated with the preceding extent even when blocks from other streams are physically interleaved. When a continuation block `C` fills, another continuation `C_cont` block is added. See [README-CONSTRAINTS.md](README-CONSTRAINTS.md#continuation-block-association-and-order) for the stream ordering and recovery rules.

A continuation block contains:

| Field | Size | Purpose |
|---|---:|---|
| **magic_number** | 2 bytes* | Identifies the start of the continuation block. |
| **crc16** | 2 bytes** | Detects accidental changes or corruption in the block. |
| **stream_id** | 1 byte* | Identifies the resolution stream this block belongs to. |
| **stream_sequence** | 4 bytes* | Orders this block within its stream. |
| **payload** | Variable | Stores more readings and their packed Essences using the schema of this extent's Record block. |

> Using a 4-byte instead of a 2-byte `stream_sequence` adds 2 bytes to each Continuation header. Assuming all remaining blocks are Continuation blocks, that adds about 506 bytes for 1 MiB of storage, 32 MiB for 64 GiB, and 64 MiB for 128 GiB.

Continuation blocks may be physically interleaved with blocks from other streams; `stream_sequence` keeps the logical order within each stream.

Approximate records per block, assuming each chest is entirely contained in 1 block:

Continuation block header: 9 bytes.

| Chest size | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,021 | 1 |
| 8 bytes | 510 | 5 |
| 12 bytes | 340 | 5 |
| 16 bytes | 255 | 5 |
| 20 bytes | 204 | 5 |
| 24 bytes | 170 | 5 |

> Unused bytes are the remaining payload space after storing whole records. The counts assume records are packed consecutively with no alignment padding.
## Payload

Each Record or Continuation block stores chests and packed Essences in separate areas. Chests grow forward from the start of the payload; Essences grow backward from its end:

```text
[block header] [chests grow →             ][← packed Essences]
```

The schema's `col_widths` determines each chest's column sizes in bytes. When `schema_config1` bit 1 enables `col_essence_bits`, each nonzero entry gives the number of Essence bits stored for that column in each chest. Essences are packed in chest order. Each Continuation block has its own Essence area. The schema stores the bit counts; the payload stores the Essences.

As chests are appended, write their codes backward from the payload end. When the block is finalized, store its record count. Readers use that count and the schema to find both areas. The block format still needs to define where the count is stored. Any unused gap between the areas remains unused space.

See [README-QUANTIZATION.md](README-QUANTIZATION.md) for Record and Continuation block capacity estimates with 2-bit and 4-bit Essences.

The capacity estimates below assume no Essence area; enabling it reduces the number of readings per block.

## Capacity Estimates

### Schema Entries and Continuation Blocks

Each Superblock has 4,087 bytes for append-only `schema_entries`. `col_count` is limited to 1–16, although its field is 1 byte. A larger count makes each entry larger and leaves room for fewer entries.

Assuming 4-byte alignment and no optional scale or quantization arrays: with the maximum `col_count = 16`, each entry is 44 bytes, so one Superblock holds about 92 entries.

For 12 columns of equal byte width, Continuation block payload capacity is:

| Width per column | Chest size | Records per Continuation block | Unused payload space |
|---:|---:|---:|---:|
| 1 byte | 12 bytes | 340 | 5 bytes |
| 2 bytes | 24 bytes | 170 | 5 bytes |
| 4 bytes | 48 bytes | 85 | 5 bytes |
| 8 bytes | 96 bytes | 42 | 53 bytes |

For 6 columns of equal byte width, Continuation block payload capacity is:

| Width per column | Chest size | Records per Continuation block | Unused payload space |
|---:|---:|---:|---:|
| 1 byte | 6 bytes | 680 | 5 bytes |
| 2 bytes | 12 bytes | 340 | 5 bytes |
| 4 bytes | 24 bytes | 170 | 5 bytes |
| 8 bytes | 48 bytes | 85 | 5 bytes |

### Total Storage Capacity

The table estimates capacity for records with 4-byte chests stored every 1, 10, or 30 seconds. With 8-byte chests, the record count and time to full are approximately half. Storage sizes use binary units.

| Storage size | Continuation<br>blocks | Continuation<br>header space | Records | Time to full<br>(1 sec) | Time to full<br>(10 secs) | Time to full<br>(30 secs) |
|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 0.04 KiB | 5,105 | ~1.4 hours | ~14.2 hours | ~42.5 hours |
| 128 KiB | 29 | 0.3 KiB | 29,609 | ~8.2 hours | ~3.4 days | ~10.3 days |
| 1 MiB | 253 | 2 KiB | 258,313 | ~3.0 days | ~29.9 days | ~89.7 days |
| 32 MiB | 8,189 | 72 KiB | 8.4 × 10⁶ | ~96.8 days | ~2.6 years | ~7.9 years |
| 126 MiB | 32,253 | 283 KiB | 32.9 × 10⁶ | ~1.0 years | ~10.4 years | ~31.3 years |
| 512 MiB | 131,069 | 1,152 KiB | 133.8 × 10⁶ | ~4.2 years | ~42 years | ~127 years |
| 1 GiB | 262,141 | 2,304 KiB | 267.6 × 10⁶ | ~8.5 years | ~85 years | ~254 years |

> Assumes 2 Superblocks and 1 Record block; all remaining 4 KiB blocks are Continuation blocks. This is an upper bound; multiple streams need additional Record blocks.
