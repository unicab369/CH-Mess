# Flash log design notes

## Base format

`flash_log.h` stores a packed 4-bit tag for each record slot. Eight tags share one
32-bit word. The tag table follows the block header; records start after the
class-specific table and alignment padding. Pass `FLASH_LOG_POOL_4` or
`FLASH_LOG_POOL_8` to the read/write API to select a size class. The record
markers are internal format values; the block header stores a versioned
class-specific commit. Existing flash data in the old format is not migrated;
initialization erases it when no current-format block is found.

The API manages one global log. The CH5xx port header defines its pool-state
array; applications call `flash_log_init()`, `flash_log_read(...)`, and
`flash_log_write(...)` without passing a pool-state array.

Keep the block header when simplifying the log. It records the block generation and
whether the block contains 4-byte or 8-byte payloads. Startup uses the generation
and commit marker to choose the newest complete copy after an interrupted GC.

The block layout is:

```text
[block header: generation | size-class commit]
[tag table: 8 x 4-bit logical-ID tags per 32-bit word]
[alignment padding]
[record: CRC | logical ID | record marker | fixed-size payload | alignment padding]
[record: CRC | logical ID | record marker | fixed-size payload | alignment padding]
...
```

Each tag belongs to a record's position in the record area. Record strides remain
class-specific: 12 bytes for 4-byte payloads and 16 bytes for 8-byte payloads. The
tag table reserves one word for each group of eight record slots. For a 4 KB block,
that leaves room for 327 4-byte records with a 164-byte tag table, or 248 8-byte
records with a 124-byte tag table.

The size class is part of the key. A 4-byte value and an 8-byte value with the same
logical ID are independent values. Keep one active block for each size class and
use an erased block as shared GC scratch. GC copies the live records of one class,
commits the destination header, then erases the old active block. The erased source
becomes the next scratch block. With both classes active, at least three blocks are
needed to guarantee a scratch block. GC searches circularly from the source block,
so successive collections rotate through the available blocks rather than always
choosing the lowest-address spare.

### Example block rotation

The log keeps separate saved values for 4-byte payloads and 8-byte payloads.
An active block is the block currently holding the saved values for one payload
size. GC for one size leaves the other size's active block alone, so its saved
values stay available.

With 3 blocks, suppose B1 holds saved values with 4-byte payloads, B2 holds saved
values with 8-byte payloads, and B3 is erased and available for GC:

- When the 4-byte block fills, GC copies its latest saved values from B1 to B3.
  It marks B3 as the new 4-byte block, then erases B1.
- The next 4-byte GC can copy the values from B3 back to B1.
- B2 keeps the 8-byte payload values throughout this process. GC leaves B2 alone.

The 4-byte data moves between B1 and the available block. The 8-byte data remains
in B2 unless GC for 8-byte payloads moves it.

With 4 blocks, suppose B1 holds 4-byte payload values, B2 holds 8-byte payload
values, and B3 and B4 are erased and available:

- Successive 4-byte GCs can move the saved values B1 → B3 → B4 → B1.
- B2 keeps the 8-byte payload values while the 4-byte values move.
- If the 8-byte block fills, its GC moves the 8-byte values to an available block.
  It leaves the block currently holding the 4-byte values alone.

Blocks are not permanently assigned to a payload size. Each size has one block
holding its current values; GC moves those values to an available block, then
erases the old block so it can be used later.

No block stays reserved as the spare. After GC commits the copied values, it
erases the old block, which becomes available for the next GC. With 3 total blocks,
each size can rotate between 2 blocks while the other size's active block is left
alone. With 4 total blocks, each size can rotate among 3 blocks while the other
size's active block is left alone.

## Tag table behavior

The 4-bit tag is the low nibble of `logical_id`. It filters candidates during
newest-to-oldest reads; a matching tag still requires checking the full ID in the
record header. Collisions cause extra header reads, never incorrect matches.

The writer accumulates eight tags in RAM and writes their 32-bit table word only
after all eight corresponding record markers are committed. This avoids rewriting
the same flash word for each tag. A final group with fewer than eight records has
no table word yet, so reads check those tail records' full headers.

On startup, the scan recomputes tags from record headers and verifies every
complete table word. If any word is unreadable or disagrees, the block's tag table
is treated as untrusted and reads fall back to scanning full record headers. GC
builds a fresh table as it copies records and commits the destination block only
after the copied records and complete tag words are written.
