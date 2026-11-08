# Flash log read benchmark

This CH582 example compares several reader paths over the same records in reserved
CodeFlash blocks:

- a baseline that reads every record header;
- the actual `flash_log_read_fast()` path with the packed 4-bit tag table;
- one shared scanner with flash-resident 4-bit tags;
- that same scanner configured for 2-bit tags;
- that same 4-bit scanner with its tag words copied into RAM;
- that same scanner configured for 8-bit tags.

The benchmark-only 2-bit tags are stored at the end of each test block. The
8-bit tags are stored just before that table, in otherwise unused space. Sixteen
2-bit tags fit in each 32-bit word. The shared scanner reads the production
4-bit table or either benchmark table; its candidate and CRC checks are the
same for each tag width. The RAM case uses the same packed 4-bit tag words
copied from flash before timing; record headers and payloads still come from
flash.

At startup, the example erases its reserved 12 KB region, initializes the log,
and writes 128 unique IDs to each payload class. It then measures newest hits,
oldest hits, and missing IDs for both classes. Each result reports average
SysTick ticks, flash-read API calls, and requested bytes per search. Output is
printed as a fixed-width table with short `4B` and `8B` values in the `Pld`
payload column. The program prints a legend for every heading before the
tables. The `%` column is the timing change relative to the matching full-header
baseline: `+` means faster and `-` means slower. The `Headers` column counts
full record headers fetched after a possible tag match; the production reader
shows `-` because its internal header count is not instrumented. Read-call
counts are benchmark-only instrumentation in the CH5xx port. The RAM case reads
tag words from RAM, so those accesses are not included in flash-read calls or
bytes.

The dataset and queries are deliberately identical between all paths.
Readers scan backward, verify the full logical ID after a tag match, and check
the payload CRC on a hit. The 8-bit table uses twice the space and reads one
word for every four records; the 4-bit table reads one word for every eight.
The 2-bit table stores sixteen tags per word, but has more collisions because
it represents only four tag values.
The shared scanner provides the width-only comparison; comparing the production
reader with that scanner also shows the cost of the production validation path.

## CH582 results

These measurements use 128 records in each size class and average 128 searches
per case. The table compares the baseline full-header scanner with the
production `flash_log_read_fast()` implementation using 4-bit tags.

| Search | Full-header scan | Production 4-bit tags | Time reduction |
|---|---:|---:|---:|
| 4-byte oldest hit | 5,219 ticks | 4,187 ticks | 20% |
| 4-byte missing ID | 5,049 ticks | 4,069 ticks | 19% |
| 8-byte oldest hit | 5,435 ticks | 4,313 ticks | 21% |
| 8-byte missing ID | 5,113 ticks | 4,096 ticks | 20% |

For these long searches, the tag table reduced record-header reads from 128 to
8. Newest-hit searches showed little or no improvement because the baseline
already finds the record after reading one header: the 4-byte case measured 263
ticks for both readers, while the 8-byte case measured 416 ticks for the
baseline and 365 ticks for the tagged reader.

The shared scanner gives a direct tag-width comparison. In the latest run,
flash-resident 4-bit tags were about 3–4% faster than 8-bit tags for oldest and
missing-ID searches. The 4-bit scan checked 16 tag groups and read 8 record
headers due to collisions; the 8-bit scan checked 32 groups and read 1 header
for an oldest hit (zero for a missing ID).

#### Same-scanner 2-bit versus 4-bit results

| Payload | Search | 4-bit ticks | 2-bit ticks | 2-bit slower | Header reads (4-bit / 2-bit) |
|---|---|---:|---:|---:|---:|
| 4-byte | Newest hit | 321 | 328 | 7 (2%) | 1 / 1 |
| 4-byte | Oldest hit | 4,129 | 4,875 | 746 (18%) | 8 / 32 |
| 4-byte | Missing ID | 3,963 | 4,702 | 739 (19%) | 8 / 32 |
| 8-byte | Newest hit | 480 | 480 | 0 (0%) | 1 / 1 |
| 8-byte | Oldest hit | 4,258 | 5,022 | 764 (18%) | 8 / 32 |
| 8-byte | Missing ID | 3,944 | 4,708 | 764 (19%) | 8 / 32 |

The 2-bit table checks 8 tag groups instead of 16, but its 4 possible tag
values cause 32 header reads on long searches. For 128 records, it uses 32 bytes
per class versus 64 bytes for 4-bit tags. In this test, that 32-byte saving did
not compensate for the extra collisions.

### 4-bit tags in RAM versus flash

The same scanner was also run with its packed 4-bit tag words copied into RAM.
The following results compare that path with reading the same tag words from
flash. Times are SysTick ticks per search on the CH582:

| Search | Tags in flash | Tags in RAM | Time saved in RAM |
|---|---:|---:|---:|
| 4-byte newest hit | 321 | 304 | 17 ticks (5%) |
| 4-byte oldest hit | 4,129 | 3,828 | 301 ticks (7%) |
| 4-byte missing ID | 3,963 | 3,664 | 299 ticks (8%) |
| 8-byte newest hit | 480 | 456 | 24 ticks (5%) |
| 8-byte oldest hit | 4,258 | 4,002 | 256 ticks (6%) |
| 8-byte missing ID | 3,944 | 3,688 | 256 ticks (6%) |

For long searches, the RAM copy removes 16 tag-word flash reads: the 4-bit
scanner still checks the same 16 groups, but reads those words from RAM. The
test uses 128 bytes of RAM for the two maps (16 words per payload class). The
measured speed gain is modest compared with the RAM cost.

These results are specific to this CH582, build, and sequential-ID dataset;
rerun on the target with representative application IDs before treating them as
expected application performance.

Build, flash, and monitor:

```sh
make
make flash
make monitor
```

The example reserves the final three 4 KB CodeFlash blocks. Its startup erase
clears that reserved region every time it runs; keep other persistent data out
of those blocks.
