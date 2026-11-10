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


## Superblocks (B0, B1)

The 2 fixed blocks used to configure the database are copies of each other. If one block is corrupted, for example, its **`magic number`** or **`CRC`** does not match, the other block can be used to recover the configuration.

The superblock also needs a `version` number. It identifies the expected format and allows the format to change over time, such as from version 1 to version 2.

Each superblock contains:

| field | size | purpose |
|---|---:|---|
| **config** (`G`) | 1 byte | Stores database-wide configuration options. |
| **version** (`V`) | 1 byte | Identifies the superblock format. |
| **magic_number** (`M`) | 2 bytes | Identifies the start of the superblock header. |
| **crc16** (`C`) | 4 bytes** | Validates the fixed superblock fields; schema slots have their own CRCs. |
| **flags** (`F`) | 1 byte | Stores superblock flags. |
| **schema_slots** (`S`) | Remaining space | Append-only schema entries. Each entry has its own CRC and commit marker. |

In the field lists:

- `*` means the field is required.
- `(*)` means the field is required, but its size still needs to be determined.
- `**` means the field size is configurable.

This shows how the superblock is stored in 2 copies, B0 and B1. Each copy contains the same fields:
```text
[B0: M, C, G, V, F, S]
[B1: M, C, G, V, F, S]
```

Schema slots store the payload layouts referenced by record blocks. `column_widths` uses 1 byte per column, and `column_signs` is sized from `column_count` at one bit per column. Their structure, write procedure, and capacity constraints are documented in [README-CONSTRAINTS.md](README-CONSTRAINTS.md#schema-log-slots).

> New data uses the new schema ID and **must be written to a new block**. A schema change is a hard boundary: the old extent ends and a new one begins. Since an extent starts with a record block, starting one always requires writing at least one new block.

## Record blocks (R)

Record blocks (`R0`–`Rn`) follow superblocks `B0` and `B1` in the sequential storage log. Each record block uses a schema ID whose entry is committed in both superblocks.

A record block contains:

| field | size | purpose |
|---|---:|---|
| **crc16** | 2 bytes* | Detects accidental changes or corruption in the block. |
| **magic_number** | 2 bytes | Identifies the start of the record block. |
| **schema_id** | 1 byte | Identifies the schema used to interpret the payload. |
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

Continuation blocks store records that follow a record block `R` when its payload is full. When a continuation block `C` fills, another continuation `C_cont` block is added. The sequence ends when a new record block `R_new` is written, such as after a schema change or recovery from power loss. Continuation blocks avoid repeating the record-block metadata for every block of the same extent.

A continuation block contains:

| Field | Size | Purpose |
|---|---:|---|
| **magic_number** | 2 bytes<sup>*</sup> | Identifies the start of the continuation block. |
| **crc16** | 2 bytes<sup>(*)</sup> | Detects accidental changes or corruption in the block. |
| **payload** | Variable | Stores records that continue from the preceding block. |

Each record block is followed by any continuation blocks needed to store its remaining records.

```text
[B0] [B1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Each bracket represents one 4 KiB block.
