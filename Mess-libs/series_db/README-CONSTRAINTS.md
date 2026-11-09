# Series DB format constraints to define


1. **Schema table limits:** Define schema table limits and encoding. Each entry currently uses 11 bytes, and the table length includes the 4-byte header. The fixed 8-byte `column_widths` array and 8 sign bits allow at most 8 columns; define how unused width slots are represented.

   | `table_length` size | Maximum table length | Maximum schema entries |
   |---|---:|---:|
   | 1 byte | 255 bytes | 22 |
   | 2 bytes | 65,535 bytes | 5,957 |

   These limits are based on table length alone. The current 1-byte `schema_count` limits the count to 255 entries, and the superblock's physical capacity may impose a lower limit.
2. **Payload and continuation blocks:** Define how readers determine the valid payload length or record count, and how continuation blocks are associated with and ordered after their record block.

## Superblock Recovery

Define the superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.

## Block Capacity

Clarify how the stated 4,094 bytes of data relate to a 4 KiB block and its headers.

## Record Configuration

`record_config` occupies 7 bytes:

| field | size | purpose |
|---|---:|---|
| `time_value` | 4 bytes | Stores the time value for records in the block. |
| `time_format` | 1 byte | Identifies how the time value is represented. |
| `flags` | 1 byte | Stores record configuration flags. |
| `codec` | 1 byte | Identifies the codec used for the payload. |
