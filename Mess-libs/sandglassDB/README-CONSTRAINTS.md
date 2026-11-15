# Series DB format constraints to define

1. **Schema log entries:** Define flash alignment and recovery for interrupted or mismatched schema-entry writes in `S0` and `S1`.

2. **Streams and block recovery:** Define the `stream_id` registry and how the shared block pool tracks free blocks. Readers group blocks by `stream_id` and order them by `stream_sequence`. Within a stream, a valid Record block starts an extent and subsequent Continuation blocks extend it. Define how readers and the allocator distinguish erased/free blocks from allocated but invalid blocks, and how sequence gaps or invalid blocks affect recovery. Cleanup or compaction must preserve stream order or rewrite the stream with a new sequence.

3. **Superblock recovery:** Define how to update and recover `S0` and `S1`. If both copies are valid but differ, specify how to select the current copy; consider a generation counter or sequence number.
