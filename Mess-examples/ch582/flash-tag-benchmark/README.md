# Flash tag scan benchmark

This CH582 example compares backward lookup using packed 4-bit tags and
8-bit tags. The test IDs and tag tables are `const`, so they are read from
CodeFlash. The benchmark also checks the full ID whenever a tag matches.

It tests 341 records for the 4-byte payload class and 255 for the 8-byte class,
matching the maximum record counts in a 4 KB block without tag metadata. It
reports SysTick ticks per search, the number of full-ID checks, and the matching
record index for:

- a hit at the newest record;
- a hit at the oldest record;
- a missing ID that collides with 4-bit tags.

Each candidate check reads both 32-bit words of an 8-byte record header from
CodeFlash. The tags are the low 4 or 8 bits of sequential IDs, so the test
deliberately shows the extra candidates caused by 4-bit collisions. It measures
tag scanning and full-header reads, but not the `HW_FLASH_READ()` wrapper, CRC,
or the complete flash-log read path. Lower ticks per search is faster.

Build and run it with:

```sh
make
make flash
make monitor
```

## CH582 results

The following results were reported by the benchmark on a CH582. Each time is
the average SysTick ticks per search over 512 runs. “ID checks” counts how many
full 8-byte record headers were read after a tag matched.

| Payload class | Search case | 4-bit ticks | 4-bit ID checks | 8-bit ticks | 8-bit ID checks |
|---|---|---:|---:|---:|---:|
| 4-byte | Newest hit | 39 | 1 | 38 | 1 |
| 4-byte | Oldest hit | 4033 | 22 | 4275 | 2 |
| 4-byte | Missing ID | 4098 | 21 | 4266 | 1 |
| 8-byte | Newest hit | 39 | 1 | 38 | 1 |
| 8-byte | Oldest hit | 3012 | 16 | 3191 | 1 |
| 8-byte | Missing ID | 3057 | 15 | 3185 | 0 |

### Interpretation

The newest-hit searches are effectively tied. In the longer searches, 4-bit tags
were about 4–6% faster even though they caused more full-header checks. Their
metadata is twice as dense: one 32-bit CodeFlash word holds eight 4-bit tags,
compared with four 8-bit tags. In this run, fewer tag-table reads outweighed the
extra header checks from 4-bit collisions.

For this test, 4-bit tags provide the better speed and space tradeoff. The test
uses sequential IDs and their low bits as tags, so other ID patterns or a hash
function may produce different collision counts. It reads both header words
directly from CodeFlash but does not call `HW_FLASH_READ()`, calculate CRCs, or
run the complete log lookup path; those costs could change the timing comparison.
