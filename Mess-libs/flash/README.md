# Flash log design notes

## Base format

`flash_log.h` currently uses this format without a per-record metadata table. Its
read and GC paths scan record headers directly. `FLASH_RECORD_MARKER_4` and
`FLASH_RECORD_MARKER_8` select the size class for the read/write API; the block
header uses matching class-specific commit values. Existing flash data in the old
format is not migrated by this version.

The API manages one global log. The CH5xx port header defines its pool-state
array; applications call `flash_log_init()`, `flash_log_read(...)`, and
`flash_log_write(...)` without passing a pool-state array.

Keep the block header when simplifying the log. It records the block generation and
whether the block contains 4-byte or 8-byte payloads. Startup uses the generation
and commit marker to choose the newest complete copy after an interrupted GC.

The base block layout should be:

```text
[block header: generation | size-class commit]
[record: CRC | logical ID | record marker | fixed-size payload | alignment padding]
[record: CRC | logical ID | record marker | fixed-size payload | alignment padding]
...
```

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

## Re-adding per-record metadata later

The per-record metadata table is an optional speed optimization. The record headers
and payload CRCs remain authoritative; metadata must only help locate records or
skip obsolete records. If restoring it:

1. Put the table after the block header and before the records. Calculate the table
   size and record start offset from the maximum number of records for that block's
   fixed payload size.
2. Give the block format a new commit marker/version. The table changes record
   offsets, so old blocks must not be interpreted using the new layout. Erase or
   migrate old data before enabling the new format.
3. Program a record and its commit marker first. Update the metadata only after the
   record is complete. A failed metadata write must make reads and GC use a full
   record scan; it must never hide a committed record.
4. Treat metadata as untrusted until checked against record headers. Before acting
   on an ID tag or live bit, confirm the record's full logical ID and size class.
   Tag collisions are hints to keep scanning, not proof of a match.
5. On an update, mark the previous record obsolete only when both the logical ID
   and size class match. Flash metadata updates must only clear programmed bits;
   restoring a cleared bit requires erasing the block.
6. During GC, copy every current record of the selected size class, write its new
   metadata, and commit the destination block last. Keep the old block intact until
   that commit succeeds. Startup can then select the newest committed generation
   and reclaim an interrupted or older copy.

For the current fixed 4-byte and 8-byte classes, a useful metadata byte could hold
one live bit and a short logical-ID tag. The block header already supplies the
payload size, so a per-record length bit is redundant. The tag is only a filter:
always confirm the complete ID in the record header before returning data or
marking a record obsolete.
