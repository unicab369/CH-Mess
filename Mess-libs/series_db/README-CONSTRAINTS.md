# Series DB format constraints to define

1. **Byte layout and CRC:** Specify byte order, field order, and CRC coverage for each block. Define the CRC-16 variant and whether the CRC field itself is excluded from the calculation.
2. **Schema ID width:** Resolve the mismatch: schema entries use a 1-byte `schema_id`, while record blocks use a 2-byte Schema ID.
3. **Schema table limits:** Define schema table limits and encoding. Each entry currently uses 11 bytes, and the 1-byte table length allows roughly 22–23 entries depending on whether it includes the table header. The fixed 8-byte `column_widths` array and 8 sign bits allow at most 8 columns; define how unused width slots are represented.
4. **Payload and continuation blocks:** Define how readers determine the valid payload length or record count, and how continuation blocks are associated with and ordered after their record block.
5. **Superblock recovery:** Define the superblock update and recovery procedure. If both copies are valid but differ, specify how to choose the current copy; consider a generation counter or sequence number.
6. **Timing configuration:** Define the 8-byte Timing Config layout and explain how it relates to timestamps in the payload.
7. **Block capacity:** Clarify how the stated 4,094 bytes of data relate to a 4 KiB block and its headers.
