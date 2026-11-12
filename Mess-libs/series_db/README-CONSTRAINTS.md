# Series DB format constraints to define


1. **Schema log entries:** Define the flash alignment and recovery procedure for append-only schema entries in S0 and S1. `schema_id`, `col_count`, and `schema_config` are fixed at 1 byte. Each entry contains these fields, the per-column width data, optional unit and scale arrays, an optional sign bitset, a 2-byte entry CRC, a 1-byte commit marker, and alignment padding. Append the same entry to both Superblocks and do not let a record block reference it until both copies validate it. The fixed Superblock CRC excludes the schema entries. Entry capacity is `floor(4087 / aligned_entry_size)`. Define recovery for interrupted or mismatched entry writes. `col_count` determines the per-column array sizes and, when present, the sign-bitset size.
2. **Payload length:** TODO — still needs consideration. Proposed: add a 2-byte `payload_length` to each block. Set it once when closing the block, include it in the CRC, and treat the block as invalid if power loss occurs before it is finalized. Readers use it to determine where valid payload data ends.
3. **Resolution streams and shared block pool:** Store each resolution (for example, raw seconds, 5-minute, 10-minute, 30-minute, and hourly summaries) as a separate logical stream. All streams allocate blocks from one shared pool rather than reserving fixed capacity per resolution. Each Record and Continuation block stores a 1-byte `stream_id` and a 4-byte `stream_sequence`. Sequence numbers start at zero and increase by one for every new block in that stream; they must not wrap. Readers group blocks by `stream_id` and order them by sequence; Continuation blocks extend the preceding extent within that stream, even when blocks from other streams are interleaved physically. Define the `stream_id` registry and free-block allocation and recovery rules.

## Schema log entries

Schema definitions are stored as append-only entries in the unused space of both Superblocks. Each new schema is appended after the previous entry in S0 and S1. A record block may use the new `schema_id` only after the entry is committed and validated in both copies. This avoids erasing and rewriting the fixed Superblock fields for each schema change.

The CRC for the fixed Superblock fields does not cover the append-only entries; each entry has its own CRC and commit marker. Readers scan the entries in both copies and build a lookup from `schema_id` to payload layout.

Each schema entry contains:

| Field | Size | Purpose |
|---|---:|---|
| **entry_crc** | 2 bytes* | Validates the schema entry. |
| **commit_marker** | 1 byte* | Marks the entry as fully written. |
| **schema_config** | 1 byte* | Selects column width units and byte order, and indicates optional per-column fields; see [Schema config](#schema-config). |
| **schema_config_ext1** | 1 byte** | Present when `schema_config` bit 7 is `1`; enables extended options below. |
| **schema_id** | 1 byte* | Identifies the schema entry. |
| **col_count** | 1 byte* | Number of columns in the schema. |
| **col_widths** | 1 byte** / column | One width value per column (`col_count` bytes); units are bytes or bits per `schema_config` bit 1. |
| **col_units** | 1 byte** / column | Present when `schema_config` bit 4 is `1`; one unit code per column. |
| **col_scale_exp** | 1 signed byte** / column | Present when `schema_config` bit 2 is `1`; one base-10 exponent per column (default `0`). E.g., `123` with exponent `-2` becomes `1.23`. |
| **col_scales** | 2 bytes** / column | Present when `schema_config` bit 3 is `1`; one numerator/denominator pair per column (default `1/1`). E.g., `10` with scale `3/2` becomes `15`. |
| **col_scale_mul** | 1 byte** / column | Present when `schema_config` bit 1 is `1`; one 1-byte multiplier per column (default `1`). E.g., `10` with multiplier `3` becomes `30`. |
| **col_signs** | `ceil(col_count / 8)` bytes** | Present when `schema_config` bit 0 is `1`; one bit per column (`1` = signed). Unused bits are `0`. |
| **padding** | Variable | Aligns the entry to the flash program unit. |


### Schema config

| Bit(s) | Meaning | Purpose |
|---|---|---|
| 7 | `1` = yes; `0` = no. | Indicates whether the 1-byte `schema_config_ext1` follows. |
| 6 | `0` = little-endian; `1` = big-endian. | Sets byte order for multi-byte column values. |
| 5 | `0` = bytes; `1` = bits. | Sets the unit used by `col_widths`. |
| 4 | `1` = yes; `0` = no (unspecified). | Indicates whether `col_units` follows. |
| 2 | `1` = yes; `0` = no (exponent defaults to `0`). | Indicates whether `col_scale_exp` follows. |
| 3 | `1` = yes; `0` = no (scale defaults to `1/1`). | Indicates whether `col_scales` follows. |
| 1 | `1` = yes; `0` = no (multiplier defaults to `1`). | Indicates whether `col_scale_mul` values follow. |
| 0 | `1` = yes; `0` = no (all columns unsigned). | Indicates whether `col_signs` follows. |

When `schema_config` bit 7 is `1`, a 1-byte `schema_config_ext1` follows `schema_config` in the schema entry:

| Bit | Meaning | Purpose |
|---:|---|---|
| 7 | Future extension flag; write `0`. | Will indicate whether `schema_config_ext2` follows. |
| 6–3 | Reserved; write `0`. | Reserved for future options. |
| 2–0 | `000` = off; `001` = 1 bit; `010` = 2 bits; `011` = 3 bits; `100` = 4 bits; `101`–`111` reserved. | Selects quantization width. |

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
schema_config   = 0b00100001  # widths are in bits; col_signs is present
col_widths = [8, 16, 32]
col_signs  = 0b01000000
```

This describes an unsigned 8-bit column, a signed 16-bit column, and an unsigned 32-bit column. Each schema change appends a new entry to both Superblocks. Existing extents retain their original `schema_id`; new data using a new schema starts in a new record block.

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

## Superblock Recovery

Define the Superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
