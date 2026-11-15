# Series DB format constraints to define


1. **Schema log entries:** Define the flash alignment and recovery procedure for append-only schema entries in S0 and S1. `schema_id`, `col_count`, `schema_config1`, and `codec` are fixed at 1 byte. Each entry contains these fields, per-column width and unit data, a sign bitset, optional scale, Essence-width, and Rune arrays, a 2-byte entry CRC, a 1-byte commit marker, and alignment padding. Append the same entry to both Superblocks and do not let a record block reference it until both copies validate it. The fixed Superblock CRC excludes the schema entries. Entry capacity is `floor(4087 / aligned_entry_size)`. Define recovery for interrupted or mismatched entry writes. `col_count` determines the per-column array sizes, including the sign-bitset size.
2. **Payload length and record count:** TODO — still needs consideration. A 2-byte `payload_length` could identify valid bytes after block finalization, but the 2-area payload also needs a record count and a defined code-area boundary. Finalize these values with the block CRC; treat an interrupted finalization as invalid.
3. **Resolution streams and shared block pool:** Store each resolution (for example, raw seconds, 5-minute, 10-minute, 30-minute, and hourly summaries) as a separate logical stream. All streams allocate blocks from one shared pool rather than reserving fixed capacity per resolution. Each Record and Continuation block stores a 1-byte `stream_id` and a 4-byte `stream_sequence`. Sequence numbers start at zero and increase by one for every new block in that stream; they must not wrap. Readers group blocks by `stream_id` and order them by sequence; Continuation blocks extend the preceding extent within that stream, even when blocks from other streams are interleaved physically. Define the `stream_id` registry and free-block allocation and recovery rules.

## Continuation Block Association and Order

- Blocks from different resolution streams may be interleaved in physical address order. Readers group blocks by `stream_id` and use the per-stream block sequence to establish logical order.
- Within each stream, a valid Record block starts an extent. A Continuation block extends the most recent valid Record extent in that stream, provided its sequence follows the previous block in that stream.
- An invalid block or sequence gap ends the current extent for that stream. Ignore that stream's Continuation blocks until its next valid Record block; continue scanning blocks belonging to other streams.
- Readers scan the full block region because an erased/free block does not end every stream's log. Define how readers and the allocator distinguish erased/free blocks from allocated but invalid blocks.
- Cleanup or compaction must preserve each stream's sequence or rewrite that stream as a new sequence.

## Superblock Recovery

Define the Superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
