# Series DB format constraints to define


1. **Schema table limits:** Define schema table limits and encoding. Each entry currently uses 11 bytes, and the table length includes the 4-byte header. The fixed 8-byte `column_widths` array and 8 sign bits allow at most 8 columns; define how unused width slots are represented.

   | `table_length` size | Maximum table length | Maximum schema entries |
   |---|---:|---:|
   | 1 byte | 255 bytes | 22 |
   | 2 bytes | 65,535 bytes | 5,957 |

   These limits are based on table length alone. The current 1-byte `schema_count` limits the count to 255 entries, and the superblock's physical capacity may impose a lower limit.
2. **Payload length:** TODO — still needs consideration. Proposed: add a 2-byte `payload_length` to each block. Set it once when closing the block, include it in the CRC, and treat the block as invalid if power loss occurs before it is finalized. Readers use it to determine where valid payload data ends.

## Continuation Block Association and Order

- Blocks are stored sequentially by design. A valid Record block starts an extent.
- Valid Continuation blocks immediately following a Record block belong to that extent and are read in ascending block-address order.
- The next valid Record block starts a new extent.
- A valid Continuation block extends the current extent only when it immediately follows a valid Record or Continuation block.
- An invalid block ends the current extent. Continue scanning at the next block boundary, but ignore Continuation blocks until the next valid Record block.
- An erased block marks the end of the log; stop scanning there.
- Cleanup or compaction must preserve this order or rewrite the blocks as a new sequential sequence.

## Record Configuration

`record_config` occupies 7 bytes:

| field | size | purpose |
|---|---:|---|
| `time_value` | 4 bytes | Stores the time value for records in the block. |
| `time_format` | 1 byte | Identifies how the time value is represented. |
| `flags` | 1 byte | Stores record configuration flags. |
| `codec` | 1 byte | Identifies the codec used for the payload. |

## Block Capacity

Assumption: the stated 4,094-byte capacity includes each block's header.

### Record block

Header:

| Header field | Size |
|---|---:|
| `magic_number` | 2 bytes |
| `crc_16` | 2 bytes |
| `schema_id` | 1 byte |
| `record_config` | 7 bytes |
| **Header total** | **12 bytes** |

Payload capacity: 4,082 bytes.

Approximate records per block, assuming each record is entirely contained in one block:

| Payload size per record | Records per block | Unused payload bytes |
|---:|---:|---:|
| 4 bytes | 1,020 | 2 |
| 8 bytes | 510 | 2 |

### Continuation block

Header:

| Header field | Size |
|---|---:|
| `magic_number` | 2 bytes |
| `crc_16` | 2 bytes |
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

\* Assumes two superblocks and one initial record block; all remaining 4 KiB blocks are continuation blocks. These estimates exclude records stored in record blocks and assume no additional blocks are needed for other purposes. Durations use 365.25 days per year.

Estimated records and time until full for periodic data, using binary storage units and the same assumptions:

| Storage size | 4-byte records | Time until full (4-byte records) | 8-byte records | Time until full (8-byte records) |
|---:|---:|---|---:|---|
| 256 MiB | 66,974,726 | 10s: ~21.2 years; 30s: ~63.7 years; 1m: ~127.3 years | 33,487,363 | 10s: ~10.6 years; 30s: ~31.8 years; 1m: ~63.7 years |
| 512 MiB | 133,952,518 | 10s: ~42.4 years; 30s: ~127.3 years; 1m: ~254.7 years | 66,976,259 | 10s: ~21.2 years; 30s: ~63.7 years; 1m: ~127.3 years |
| 1 GiB | 267,908,102 | 10s: ~84.9 years; 30s: ~254.7 years; 1m: ~509.4 years | 133,954,051 | 10s: ~42.4 years; 30s: ~127.3 years; 1m: ~254.7 years |
| 4 GiB | 1,071,641,606 | 10s: ~339.6 years; 30s: ~1,018.7 years; 1m: ~2,037.5 years | 535,820,803 | 10s: ~169.8 years; 30s: ~509.4 years; 1m: ~1,018.7 years |
| 8 GiB | 2,143,286,278 | 10s: ~679.2 years; 30s: ~2,037.5 years; 1m: ~4,075.0 years | 1,071,643,139 | 10s: ~339.6 years; 30s: ~1,018.7 years; 1m: ~2,037.5 years |

Each duration assumes one record is stored at the stated interval. These are storage-capacity estimates, not flash-lifetime estimates.

## Superblock Recovery

Define the superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
