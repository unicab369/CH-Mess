# Series DB format constraints to define


1. **Schema log slots:** Define the flash alignment and recovery procedure for append-only schema slots in B0 and B1. `schema_id`, `col_count`, and `slot_config` are fixed at 1 byte. Each slot contains these fields, the per-column width data, optional unit and scale arrays, an optional sign bitset, a 2-byte entry CRC, a 1-byte commit marker, and alignment padding. Append the same entry to both superblocks and do not let a record block reference it until both copies validate it. The fixed superblock CRC excludes the slots. Slot capacity is `floor(4087 / aligned_slot_size)`. Define recovery for interrupted or mismatched slot writes. `col_count` determines the per-column array sizes and, when present, the sign-bitset size.
2. **Payload length:** TODO — still needs consideration. Proposed: add a 2-byte `payload_length` to each block. Set it once when closing the block, include it in the CRC, and treat the block as invalid if power loss occurs before it is finalized. Readers use it to determine where valid payload data ends.

## Schema log slots

Schema definitions are stored in append-only slots in the unused space of both superblocks. Each new schema is appended to the next slot in B0 and B1. A record block may use the new `schema_id` only after the entry is committed and validated in both copies. This avoids erasing and rewriting the fixed superblock fields for each schema change.

The CRC for the fixed superblock fields does not cover the append-only slots; each slot has its own CRC and commit marker. Readers scan the slots in both copies and build a lookup from `schema_id` to payload layout.

Each schema slot contains:

| Field | Size | Purpose |
|---|---:|---|
| **entry_crc** | 2 bytes* | Validates the schema entry. |
| **commit_marker** | 1 byte* | Marks the entry as fully written. |
| **slot_config** | 1 byte* | Selects column width units and byte order, and indicates optional per-column fields; see [Slot config](#slot-config). |
| **schema_id** | 1 byte* | Identifies the schema entry. |
| **col_count** | 1 byte* | Number of columns in the schema. |
| **col_widths** | 1 byte per column** | Stores one width value per column; total size is `col_count` bytes. `slot_config` determines whether each width value is in bytes or bits. |
| **col_units** | Optional; 1 byte per column | Present when `slot_config` bit 4 is `1`; each byte is a unit code from the table below. If absent, units are unspecified. |
| **col_scale_exp** | Optional; 1 signed byte per column | Present when `slot_config` bit 5 is `1`; stores a base-10 exponent for each column. If absent, the exponent is `0`. |
| **col_scales** | Optional; 2 bytes per column | Present when `slot_config` bit 3 is `1`; stores a 1-byte numerator and 1-byte denominator for each column. If absent, the ratio is `1/1`. |
| **col_signs** | Optional; `ceil(col_count / 8)` bytes | Present when `slot_config` bit 0 is `1`; one bit per column (`1` = signed, `0` = unsigned). If absent, all columns are unsigned. Unused high bits in the last byte are zero. |
| **padding** | Variable | Aligns the slot to the flash program unit. |


### Slot config

| Bit(s) | Meaning |
|---|---|
| 7 | Reserved for future extension of `slot_config`; write as `0` for now. |
| 6 | Byte order for multi-byte column values: `0` = little-endian; `1` = big-endian. |
| 5 | Reserved; write as `0`. |
| 4 | `1` = `col_units` is present; `0` = omitted and units are unspecified. |
| 3 | `1` = `col_scales` is present; `0` = omitted and scales are `1/1`. |
| 2 | `1` = `col_scale_exp` is present; `0` = omitted and all exponents are zero. |
| 1 | `0` = `col_widths` are in bytes; `1` = `col_widths` are in bits. |
| 0 | `1` = `col_signs` is present; `0` = omitted and all columns are unsigned. |

Bit 7 is reserved for a possible future extension of `slot_config`; no extension byte is defined in the current format.

For each column, interpret the numeric value as `stored_value × numerator / denominator × 10^exponent`. For example, `3/1` multiplies by 3, `1/4` divides by 4, and an exponent of `-2` divides by 100. Both numerator and denominator must be nonzero. If either optional field is absent, use its neutral value (`1/1` for the ratio and `0` for the exponent). Apply the scaling before interpreting the value in its column unit.

Unit codes are 1 byte and are assigned as follows:

| Codes | Units, in code order |
|---:|---|
| `0–2` | Unspecified, Count, Thaum |
| `3–4` | Percent (%), Parts per million (ppm) |
| `5–9` | Lux, Decibel milliwatt (dBm), Decibel (dB), Degree (°), Radian |
| `10–12` | Kelvin (K), Celsius (°C), Fahrenheit (°F) |
| `13–16` | Inch, Foot, Yard, Mile |
| `17–19` | Ounce, Pound, Short ton (US) |
| `20–22` | Nanosecond, Microsecond, Millisecond |
| `23–29` | Second, Minute, Hour, Day, Calendar month, Calendar year, Week |
| `30–32` | Hertz (Hz), kilohertz (kHz), gigahertz (GHz) |

Codes are stable identifiers and must not be reused for different units. Unassigned codes are reserved for future units.

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
slot_config   = 0b00000011  # widths are in bits; col_signs is present
col_widths = [8, 16, 32]
col_signs  = 0b01000000
```

This describes an unsigned 8-bit column, a signed 16-bit column, and an unsigned 32-bit column. Each schema change appends a new slot to both superblocks. Existing extents retain their original `schema_id`; new data using a new schema starts in a new record block.

## Schema Slot Capacity Estimate

Assumptions: 4-byte flash write alignment; `col_units`, `col_scale_exp`, and `col_scales` are absent; `col_signs` is present; 1-byte `schema_id`, `col_count`, `slot_config`, and `commit_marker`; 2-byte entry CRC; `col_widths` uses 1 byte per column and `col_signs` uses one bit per column. The fixed superblock fields leave 4,087 bytes for slots. Both superblocks store the same entries, so their capacities are not added together.

| `col_count` | `col_widths` size | Slot size after padding | Maximum schema entries | Space per superblock |
|---:|---:|---:|---:|---:|
| 1 | 1 byte | 8 bytes | 256 | 2,048 bytes |
| 2 | 2 bytes | 12 bytes | 256 | 3,072 bytes |
| 4 | 4 bytes | 12 bytes | 256 | 3,072 bytes |
| 8 | 8 bytes | 16 bytes | 254 | 4,064 bytes |

Maximum schema entries counts one slot per distinct schema ID; each slot's size includes its own `schema_id` field.

Padding is only for flash write alignment; it is not required by the schema encoding itself. These estimates assume the target flash requires 4-byte alignment, which must be verified for the MCU. If byte-granular writes are supported, padding may be unnecessary and the slot capacity should be recalculated.

The 1-byte `schema_id` caps the number of distinct schemas at 256. With 8 columns, `col_widths` uses 8 bytes and `col_signs` uses 1 byte, making each slot 16 bytes when aligned to 4 bytes. This allows 254 schema entries or 253 changes after the initial schema. Confirm the assumed 4-byte write alignment for the target flash.

## Continuation Block Association and Order

- Blocks are stored sequentially by design and scanned in ascending block-address order. Each valid Record block starts an extent; valid Continuation blocks immediately following it extend that extent until the next Record block.
- An invalid block ends the current extent. Continue scanning at the next block boundary, but ignore Continuation blocks until the next valid Record block.
- An erased block marks the end of the log; stop scanning there.
- Cleanup or compaction must preserve this order or rewrite the blocks as a new sequential sequence.

## Record Configuration

`record_config` occupies 6 bytes:

| field | size | purpose |
|---|---:|---|
| `time_value` | 4 bytes | Stores the time value for records in the block. |
| `time_format` | 1 byte | Identifies how the time value is represented. |
| `flags` | 1 byte (8 bits) | Stores record configuration flags. Bits 7–5 encode the codec; bits 4–0 are reserved. |

```text
bit:    7   6   5   4   3   2   1   0
      +-----------+-------------------+
      | codec (3) |    reserved (5)   |
      +-----------+-------------------+
```

Codec IDs are defined as follows:

| ID | codec | meaning |
|---:|---|---|
| `0` | Raw | Store records unchanged according to the schema; default. |
| `1` | Run-length encoding (RLE) | Store repeated identical records once with a count. |
| `2` | Delta + varint | Store the first record raw, then signed per-field differences from the previous record as variable-length integers. The exact integer encoding and field ordering must be specified before implementation. |
| `3`–`7` | Reserved | Do not write; readers should reject blocks using these IDs as unsupported. |

The codec applies to the payload across the record block and its continuation blocks. Until non-raw decoding and record-boundary rules are implemented, write codec ID `0`. Reserved flag bits must be written as zero and ignored by readers.

## Block Capacity

Assumption: the stated 4,094-byte capacity includes each block's header.

### Record block

Header:

| Header field | Size |
|---|---:|
| `magic_number` | 2 bytes* |
| `crc16` | 2 bytes** |
| `schema_id` | 1 byte* |
| `record_config` | 6 bytes |
| **Header total** | **11 bytes** |

Payload capacity: 4,083 bytes.

Approximate records per block, assuming each record is entirely contained in one block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,020 | 2 |
| 8 bytes | 510 | 2 |

### Continuation block

Header:

| Header field | Size |
|---|---:|
| `magic_number` | 2 bytes* |
| `crc16` | 2 bytes** |
| **Header total** | **4 bytes** |

Payload capacity: 4,090 bytes.

Approximate records per block, assuming each record is entirely contained in one block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,022 | 2 |
| 8 bytes | 511 | 2 |

Unused bytes are the remaining payload space after storing whole records. The counts assume records are packed consecutively with no alignment padding.

Estimated continuation-block record capacity by storage size (using binary GiB, where 1 GiB = 1,073,741,824 bytes):

| Storage size | Continuation blocks* | 4-byte records | 4-byte duration (1 record/sec) | 8-byte records | 8-byte duration (1 record/sec) |
|---:|---:|---:|---:|---:|---:|
| 1 GiB | 262,141 | 267,908,102 | ~8.49 years | 133,954,051 | ~4.24 years |
| 16 GiB | 4,194,301 | 4,286,575,622 | ~135.83 years | 2,143,287,811 | ~67.92 years |
| 32 GiB | 8,388,605 | 8,573,154,310 | ~271.67 years | 4,286,577,155 | ~135.83 years |

\* Assumes two superblocks and one initial record block; all remaining 4 KiB blocks are continuation blocks. These estimates exclude records stored in record blocks and assume no other blocks are needed. Durations use 365.25 days per year.

Estimated records and time until full for periodic data, using binary storage units and the same assumptions:

| Storage size | 4-byte records | Time until full<br>(10 secs \| 30 secs) | 8-byte records | Time until full<br>(10 secs \| 30 secs) |
|---:|---:|---|---:|---|
| 126 MiB | 32,962,566 | 11 years \| 32 years | 16,481,283 | 6 years \| 16 years |
| 256 MiB | 66,974,726 | 22 years \| 64 years | 33,487,363 | 11 years \| 32 years |
| 512 MiB | 133,952,518 | 43 years \| 128 years | 66,976,259 | 22 years \| 64 years |
| 1 GiB | 267,908,102 | 85 years \| 255 years | 133,954,051 | 43 years \| 128 years |

Each duration assumes one record is stored at the stated interval and is rounded up to a whole year. These are storage-capacity estimates, not flash-lifetime estimates.

## Superblock Recovery

Define the superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
