# Series DB

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

## Superblocks (S0, S1)

The 2 fixed blocks used to configure the database are copies of each other. If one block is corrupted, for example, its **`magic number`** or **`CRC`** does not match, the other block can be used to recover the configuration.

The Superblock also needs a `version` number. It identifies the expected format and allows the format to change over time, such as from version 1 to version 2.

Each Superblock contains:

| field | size | purpose |
|---|---:|---|
| **config** (`G`) | 1 byte* | Stores database-wide configuration options. |
| **version** (`V`) | 1 byte* | Identifies the Superblock format. |
| **magic_number** (`M`) | 2 bytes* | Identifies the start of the Superblock header. |
| **crc16** (`C`) | 4 bytes** | Covers `config`, `version`, `magic_number`, and `flags`, excluding the CRC field itself and `schema_entries`. |
| **flags** (`F`) | 1 byte* | Stores Superblock flags. |
| **schema_entries** (`S`) | Remaining space | Append-only schema entries. Each entry has its own CRC and commit marker. |

The Superblock is stored in 2 copies, S0 and S1. Each copy has this field layout:
```text
       +---+---+---+---+---+---+
S0:    | G | V | M | F | C | S |
       +---+---+---+---+---+---+
S1:    | G | V | M | F | C | S |
       +---+---+---+---+---+---+
```

The Superblock `config` byte uses these bits:

| Bits | Meaning |
|---|---|
| 7–4 | Reserved; write as `0`. |
| 3–0 | Reserved; write as `0`. |


Schema entries store the payload layouts referenced by record blocks. See [README-CONSTRAINTS.md](README-CONSTRAINTS.md#schema-config) for details.

> New data uses the new schema ID and **must be written to a new block**. A schema change is a hard boundary: the old extent ends and a new one begins. Since an extent starts with a record block, starting one always requires writing at least one new block.

## Record blocks (R)

Record blocks (`R0`–`Rn`) follow Superblocks `S0` and `S1`. Each record block belongs to a logical resolution stream, identified by `stream_id`, and uses a `schema_id` whose entry is committed in both Superblocks. Different streams can allocate blocks from the same shared pool.

A record block contains:

| field | size | purpose |
|---|---:|---|
| **magic_number** | 2 bytes* | Identifies the start of the record block. |
| **crc16** | 2 bytes** | Detects accidental changes or corruption in the block. |
| **schema_id** | 1 byte* | Identifies the schema used to interpret the payload. |
| **stream_id** | 1 byte* | Identifies the resolution stream this block belongs to. |
| **stream_sequence** | 4 bytes* | Orders this block within its stream. |
| **record_config** | 6 bytes | Stores the time, format, and flags for the records. |
| **payload** | Variable | Stores the records. |

The 1-byte `flags` field contains 8 bits. Its most significant 3 bits encode the payload codec; the remaining 5 bits are reserved and must be written as zero:

```text
bit:    7   6   5   4   3   2   1   0
      +-----------+-------------------+
      | codec (3) |    reserved (5)   |
      +-----------+-------------------+
```

Codec IDs are:

| ID | codec | meaning |
|---:|---|---|
| `0` | Raw | Store records exactly as described by the schema, with no transformation. This is the default. |
| `1` | Run-length encoding (RLE) | Store a repeated record once with a count. Use only when the payload consists of consecutive identical records. |
| `2` | Delta + varint | Store the first record as a raw value, then encode each subsequent numeric value as a signed difference from the previous record, using a variable-length integer. Requires a defined field order and integer encoding. |
| `3`–`7` | Reserved | Must not be written. Readers encountering these IDs should treat the record block as unsupported. |

Codec applies to the payload of the record block and its continuation blocks. A codec must define its record boundaries and decoding rules; until those rules are specified and implemented, use ID `0` (Raw).

Approximate records per block, assuming each record is entirely contained in 1 block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,019 | 2 |
| 8 bytes | 509 | 6 |

## Continuation blocks (C)

Continuation blocks store records that follow a record block `R` when its payload is full. Each belongs to the same `stream_id` and carries a per-stream block sequence so it can be associated with the preceding extent even when blocks from other streams are physically interleaved. When a continuation block `C` fills, another continuation `C_cont` block is added. See [README-CONSTRAINTS.md](README-CONSTRAINTS.md#continuation-block-association-and-order) for the stream ordering and recovery rules.

A continuation block contains:

| Field | Size | Purpose |
|---|---:|---|
| **magic_number** | 2 bytes* | Identifies the start of the continuation block. |
| **crc16** | 2 bytes** | Detects accidental changes or corruption in the block. |
| **stream_id** | 1 byte* | Identifies the resolution stream this block belongs to. |
| **stream_sequence** | 4 bytes* | Orders this block within its stream. |
| **payload** | Variable | Stores records that continue from the preceding block. |

> Using a 4-byte instead of a 2-byte `stream_sequence` adds 2 bytes to each Continuation header. Assuming all remaining blocks are Continuation blocks, that adds about 506 bytes for 1 MiB of storage, 32 MiB for 64 GiB, and 64 MiB for 128 GiB.

Continuation blocks may be physically interleaved with blocks from other streams; `stream_sequence` keeps the logical order within each stream.

Approximate records per block, assuming each record is entirely contained in 1 block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,021 | 1 |
| 8 bytes | 510 | 5 |

> Unused bytes are the remaining payload space after storing whole records. The counts assume records are packed consecutively with no alignment padding.


```text
[S0] [S1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Each bracket represents one 4 KiB block.

## Capacity Estimates

### Schema Entries and Continuation Blocks

Each Superblock has 4,087 bytes for append-only `schema_entries`. `col_count` ranges from 1 to 255 (`0xFF` = 255). A larger count makes each entry larger and leaves room for fewer entries.

Assuming 4-byte alignment, `col_signs` present, and other optional arrays absent: with `col_count = 16`, each entry is 24 bytes, so one Superblock holds about 170 entries. With `col_count = 255`, an entry is about 296 bytes and one Superblock holds 13 entries (12 changes after the initial entry).

For 12 columns of equal byte width, Continuation block payload capacity is:

| Width per column | Record size | Records per Continuation block | Unused payload space |
|---:|---:|---:|---:|
| 1 byte | 12 bytes | 340 | 5 bytes |
| 2 bytes | 24 bytes | 170 | 5 bytes |
| 4 bytes | 48 bytes | 85 | 5 bytes |
| 8 bytes | 96 bytes | 42 | 53 bytes |

For 6 columns of equal byte width, Continuation block payload capacity is:

| Width per column | Record size | Records per Continuation block | Unused payload space |
|---:|---:|---:|---:|
| 1 byte | 6 bytes | 680 | 5 bytes |
| 2 bytes | 12 bytes | 340 | 5 bytes |
| 4 bytes | 24 bytes | 170 | 5 bytes |
| 8 bytes | 48 bytes | 85 | 5 bytes |

### Total Storage Capacity

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
