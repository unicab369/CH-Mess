# Series DB format constraints to define


1. **Schema log slots:** Define the flash alignment and recovery procedure for append-only schema slots in B0 and B1. `schema_id`, `col_count`, and `slot_config` are fixed at 1 byte. Each slot contains these fields, the per-column width data, optional unit and scale arrays, an optional sign bitset, a 2-byte entry CRC, a 1-byte commit marker, and alignment padding. Append the same entry to both superblocks and do not let a record block reference it until both copies validate it. The fixed superblock CRC excludes the slots. Slot capacity is `floor(4087 / aligned_slot_size)`. Define recovery for interrupted or mismatched slot writes. `col_count` determines the per-column array sizes and, when present, the sign-bitset size.
2. **Payload length:** TODO — still needs consideration. Proposed: add a 2-byte `payload_length` to each block. Set it once when closing the block, include it in the CRC, and treat the block as invalid if power loss occurs before it is finalized. Readers use it to determine where valid payload data ends.
3. **Resolution streams and shared block pool:** Store each resolution (for example, raw seconds, 5-minute, 10-minute, 30-minute, and hourly summaries) as a separate logical stream. All streams allocate blocks from one shared pool rather than reserving fixed capacity per resolution. Each Record and Continuation block stores a 1-byte `stream_id` and a 4-byte `stream_sequence`. Sequence numbers start at zero and increase by one for every new block in that stream; they must not wrap. Readers group blocks by `stream_id` and order them by sequence; Continuation blocks extend the preceding extent within that stream, even when blocks from other streams are interleaved physically. Define the `stream_id` registry and free-block allocation and recovery rules.

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

- Blocks from different resolution streams may be interleaved in physical address order. Readers group blocks by `stream_id` and use the per-stream block sequence to establish logical order.
- Within each stream, a valid Record block starts an extent. A Continuation block extends the most recent valid Record extent in that stream, provided its sequence follows the previous block in that stream.
- An invalid block or sequence gap ends the current extent for that stream. Ignore that stream's Continuation blocks until its next valid Record block; continue scanning blocks belonging to other streams.
- Readers scan the full block region because an erased/free block does not end every stream's log. Define how readers and the allocator distinguish erased/free blocks from allocated but invalid blocks.
- Cleanup or compaction must preserve each stream's sequence or rewrite that stream as a new sequence.

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

Each Record and Continuation block stores a 1-byte `stream_id` and a 4-byte per-stream `stream_sequence`.

### Record block

Header:

| Header field | Size | Purpose |
|---|---:|---|
| `magic_number` | 2 bytes* | Identifies the start of a Record block. |
| `crc16` | 2 bytes** | Detects accidental changes or corruption in the block. |
| `schema_id` | 1 byte* | Identifies the schema used to interpret the payload. |
| `stream_id` | 1 byte* | Identifies the resolution stream this block belongs to. |
| `stream_sequence` | 4 bytes* | Orders this block within its stream. |
| `record_config` | 6 bytes | Stores the time, format, and flags for the records. |
| **Header total** | **16 bytes** | |

Payload capacity: 4,078 bytes.

Approximate records per block, assuming each record is entirely contained in one block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,019 | 2 |
| 8 bytes | 509 | 6 |

### Continuation block

Header:

| Header field | Size | Purpose |
|---|---:|---|
| `magic_number` | 2 bytes* | Identifies the start of a Continuation block. |
| `crc16` | 2 bytes** | Detects accidental changes or corruption in the block. |
| `stream_id` | 1 byte* | Identifies the resolution stream this block belongs to. |
| `stream_sequence` | 4 bytes* | Orders this block within its stream. |
| **Header total** | **9 bytes** | |

Payload capacity: 4,085 bytes.

Approximate records per block, assuming each record is entirely contained in one block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,021 | 1 |
| 8 bytes | 510 | 5 |

Unused bytes are the remaining payload space after storing whole records. The counts assume records are packed consecutively with no alignment padding.

Estimated continuation-block record capacity by storage size (using binary GiB, where 1 GiB = 1,073,741,824 bytes):

| Storage size | Continuation blocks* | 4-byte records | 4-byte duration (1 record/sec) | 8-byte records | 8-byte duration (1 record/sec) |
|---:|---:|---:|---:|---:|---:|
| 1 GiB | 262,141 | 267.6 × 10⁶ | ~8.5 years | 133.7 × 10⁶ | ~4.2 years |
| 16 GiB | 4,194,301 | 4,282.4 × 10⁶ | ~135.7 years | 2,139.1 × 10⁶ | ~67.8 years |
| 32 GiB | 8,388,605 | 8,564.8 × 10⁶ | ~271.4 years | 4,278.2 × 10⁶ | ~135.7 years |

\* Assumes two superblocks and one initial record block for a single stream; all remaining 4 KiB blocks are continuation blocks. This is an upper bound; multiple streams need additional Record blocks. Durations use 365.25 days per year.

---

For both tables, storage sizes use binary units. Counts assume two superblocks and one Record block, with all remaining blocks used as Continuation blocks. Durations assume one record is stored every 10 or 30 seconds.

Estimated capacity for 4-byte records:

| Storage size | Continuation blocks | Continuation header space | Records | Time until full<br>(10 secs \| 30 secs) |
|---:|---:|---:|---:|---|
| 128 KiB | 29 | 0.3 KB | 0.030 × 10⁶ | ~3 days \| ~10 days |
| 1 MiB | 253 | 2 KB | 0.26 × 10⁶ | ~30 days \| ~90 days |
| 32 MiB | 8,189 | 72 KB | 8.4 × 10⁶ | 3 years \| 2 years |
| 126 MiB | 32,253 | 283 KB | 32.9 × 10⁶ | 11 years \| 32 years |
| 512 MiB | 131,069 | 1,152 KB | 133.8 × 10⁶ | 43 years \| 128 years |
| 1 GiB | 262,141 | 2,304 KB | 267.6 × 10⁶ | 85 years \| 255 years |

Estimated capacity for 8-byte records:

| Storage size | Continuation blocks | Continuation header space | Records | Time until full<br>(10 secs \| 30 secs) |
|---:|---:|---:|---:|---|
| 128 KiB | 29 | 0.3 KB | 0.015 × 10⁶ | ~2 days \| ~5 days |
| 1 MiB | 253 | 2 KB | 0.13 × 10⁶ | ~15 days \| ~45 days |
| 32 MiB | 8,189 | 72 KB | 4.2 × 10⁶ | 2 years \| 4 years |
| 126 MiB | 32,253 | 283 KB | 16.4 × 10⁶ | 6 years \| 16 years |
| 512 MiB | 131,069 | 1,152 KB | 66.8 × 10⁶ | 22 years \| 64 years |
| 1 GiB | 262,141 | 2,304 KB | 133.7 × 10⁶ | 43 years \| 128 years |

Each duration assumes one record is stored at the stated interval. Durations under a year are rounded to whole days; longer durations are rounded up to whole years. These are storage-capacity estimates, not flash-lifetime estimates.

## Superblock Recovery

Define the superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
