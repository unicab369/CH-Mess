# Series DB format constraints to define


1. **Schema log slots:** Define the flash alignment and recovery procedure for append-only schema slots in S0 and S1. `schema_id`, `col_count`, and `slot_config` are fixed at 1 byte. Each slot contains these fields, the per-column width data, optional unit and scale arrays, an optional sign bitset, a 2-byte entry CRC, a 1-byte commit marker, and alignment padding. Append the same entry to both Superblocks and do not let a record block reference it until both copies validate it. The fixed Superblock CRC excludes the slots. Slot capacity is `floor(4087 / aligned_slot_size)`. Define recovery for interrupted or mismatched slot writes. `col_count` determines the per-column array sizes and, when present, the sign-bitset size.
2. **Payload length:** TODO — still needs consideration. Proposed: add a 2-byte `payload_length` to each block. Set it once when closing the block, include it in the CRC, and treat the block as invalid if power loss occurs before it is finalized. Readers use it to determine where valid payload data ends.
3. **Resolution streams and shared block pool:** Store each resolution (for example, raw seconds, 5-minute, 10-minute, 30-minute, and hourly summaries) as a separate logical stream. All streams allocate blocks from one shared pool rather than reserving fixed capacity per resolution. Each Record and Continuation block stores a 1-byte `stream_id` and a 4-byte `stream_sequence`. Sequence numbers start at zero and increase by one for every new block in that stream; they must not wrap. Readers group blocks by `stream_id` and order them by sequence; Continuation blocks extend the preceding extent within that stream, even when blocks from other streams are interleaved physically. Define the `stream_id` registry and free-block allocation and recovery rules.

## Schema log slots

Schema definitions are stored in append-only slots in the unused space of both Superblocks. Each new schema is appended to the next slot in S0 and S1. A record block may use the new `schema_id` only after the entry is committed and validated in both copies. This avoids erasing and rewriting the fixed Superblock fields for each schema change.

The CRC for the fixed Superblock fields does not cover the append-only slots; each slot has its own CRC and commit marker. Readers scan the slots in both copies and build a lookup from `schema_id` to payload layout.

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

This describes an unsigned 8-bit column, a signed 16-bit column, and an unsigned 32-bit column. Each schema change appends a new slot to both Superblocks. Existing extents retain their original `schema_id`; new data using a new schema starts in a new record block.

## Schema Slot Capacity Estimate

Assumptions: 4-byte flash write alignment; `col_units`, `col_scale_exp`, and `col_scales` are absent; `col_signs` is present; 1-byte `schema_id`, `col_count`, `slot_config`, and `commit_marker`; 2-byte entry CRC; `col_widths` uses 1 byte per column and `col_signs` uses one bit per column. The fixed Superblock fields leave 4,087 bytes for slots. Both Superblocks store the same entries, so their capacities are not added together.

| `col_count` | `col_widths` size | Slot size after padding | Maximum schema entries | Space per Superblock |
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

### Storage Capacity Estimates

The table estimates capacity for 4-byte records stored every 1, 10, or 30 seconds. For 8-byte records, the record count and time to full are approximately half. Storage sizes use binary units.

| Storage size | Continuation<br>blocks | Continuation<br>header space | Records | Time to full<br>(1 sec) | Time to full<br>(10 secs) | Time to full<br>(30 secs) |
|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 0.04 KiB | 5,105 | ~1.4 hours | ~14.2 hours | ~42.5 hours |
| 128 KiB | 29 | 0.3 KiB | 29,609 | ~8.2 hours | ~3.4 days | ~10.3 days |
| 1 MiB | 253 | 2 KiB | 258,313 | ~3.0 days | ~29.9 days | ~89.7 days |
| 32 MiB | 8,189 | 72 KiB | 8.4 × 10⁶ | ~96.8 days | ~2.6 years | ~7.9 years |
| 126 MiB | 32,253 | 283 KiB | 32.9 × 10⁶ | ~1.0 years | ~10.4 years | ~31.3 years |
| 512 MiB | 131,069 | 1,152 KiB | 133.8 × 10⁶ | ~4.2 years | ~42 years | ~127 years |
| 1 GiB | 262,141 | 2,304 KiB | 267.6 × 10⁶ | ~8.5 years | ~85 years | ~254 years |

> Assumes 2 Superblocks and 1 Record block; all remaining 4 KiB blocks are Continuation blocks. This is an upper bound; multiple streams need additional Record blocks. Durations use 365.25 days per year.

## Superblock Recovery

Define the Superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
