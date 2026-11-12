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

## Superblocks (B0, B1)

The 2 fixed blocks used to configure the database are copies of each other. If one block is corrupted, for example, its **`magic number`** or **`CRC`** does not match, the other block can be used to recover the configuration.

The superblock also needs a `version` number. It identifies the expected format and allows the format to change over time, such as from version 1 to version 2.

Each superblock contains:

| field | size | purpose |
|---|---:|---|
| **config** (`G`) | 1 byte* | Stores database-wide configuration options. |
| **version** (`V`) | 1 byte* | Identifies the superblock format. |
| **magic_number** (`M`) | 2 bytes* | Identifies the start of the superblock header. |
| **crc16** (`C`) | 4 bytes** | Covers `config`, `version`, `magic_number`, and `flags`, excluding the CRC field itself and `schema_slots`. |
| **flags** (`F`) | 1 byte* | Stores superblock flags. |
| **schema_slots** (`S`) | Remaining space | Append-only schema entries. Each entry has its own CRC and commit marker. |

The superblock is stored in 2 copies, B0 and B1. Each copy has this field layout:
```text
       +---+---+---+---+---+---+
B0:    | G | V | M | F | C | S |
       +---+---+---+---+---+---+
B1:    | G | V | M | F | C | S |
       +---+---+---+---+---+---+
```

The superblock `config` byte uses these bits:

| Bits | Meaning |
|---|---|
| 7–4 | Reserved; write as `0`. |
| 3–0 | Reserved; write as `0`. |


Schema slots store the payload layouts referenced by record blocks. `slot_config` configures column width units, byte order, and whether optional `col_signs`, `col_units`, or scale fields are present. Bit 7 is reserved for a possible future extension. See [README-CONSTRAINTS.md](README-CONSTRAINTS.md#slot-config) for details.

> New data uses the new schema ID and **must be written to a new block**. A schema change is a hard boundary: the old extent ends and a new one begins. Since an extent starts with a record block, starting one always requires writing at least one new block.

## Record blocks (R)

Record blocks (`R0`–`Rn`) follow superblocks `B0` and `B1`. Each record block belongs to a logical resolution stream, identified by `stream_id`, and uses a `schema_id` whose entry is committed in both superblocks. Different streams can allocate blocks from the same shared pool.

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

Continuation blocks may be physically interleaved with blocks from other streams; `stream_sequence` keeps the logical order within each stream.

```text
[B0] [B1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Each bracket represents one 4 KiB block.
