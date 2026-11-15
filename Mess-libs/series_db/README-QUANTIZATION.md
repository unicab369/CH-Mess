# Quantization Capacity

Capacity estimates assume 4,094 bytes per block, a 16-byte Record header, a 9-byte Continuation header, and one quantization code per column. A <strong>chest</strong> is the stored column values for one record; its size is the sum of the column widths and excludes quantization codes. Unused payload space is shown in whole bytes; leftover bits are omitted.

Let `Payload` be the payload capacity (`4,078` bytes for a Record block; `4,085` for a Continuation block), `Chest_size` the chest size in bytes, `Q` the total quantization code bits per chest, and `N` the maximum whole chests that fit:

```text
N = largest integer such that N × Chest_size + ceil(N × Q / 8) ≤ Payload
unused whole bytes = Payload − N × Chest_size − ceil(N × Q / 8)
```

In these tables, columns scale with chest size and each column gets one code. Therefore, total chest bytes per block change mainly with quantization code width, not chest size; small differences are due to packing and rounding.

Switching from 4-bit to 2-bit codes halves the code area. For a 4-byte chest with four 1-byte columns, a Record block holds <strong>815 instead of 679 records</strong>. With one 4-byte column, it holds <strong>959 instead of 906 records</strong>. Fewer, wider columns need fewer codes.


## Quantization Capacity by Column Width

### 1-byte columns / 2-bit quantization

| Chest size | Columns per chest | Chests per block (R \| C) | Chest bytes per block (R \| C) | Quantization code bytes per block (R \| C) | Unused payload per block (R \| C) |
|---:|---:|---:|---:|---:|---:|
| 4 B | 4 | 815 \| 817 | 3,260 \| 3,268 B | 815 \| 817 B | `3 \| 0 B` |
| 8 B | 8 | 407 \| 408 | 3,256 \| 3,264 B | 814 \| 816 B | `8 \| 5 B` |
| 12 B | 12 | 271 \| 272 | 3,252 \| 3,264 B | 813 \| 816 B | `13 \| 5 B` |
| 16 B | 16 | 203 \| 204 | 3,248 \| 3,264 B | 812 \| 816 B | `18 \| 5 B` |
| 20 B | 20 | 163 \| 163 | 3,260 \| 3,260 B | 815 \| 815 B | `3 \| 10 B` |
| 24 B | 24 | 135 \| 136 | 3,240 \| 3,264 B | 810 \| 816 B | `28 \| 5 B` |
| 28 B | 28 | 116 \| 116 | 3,248 \| 3,248 B | 812 \| 812 B | `18 \| 25 B` |
| 32 B | 32 | 101 \| 102 | 3,232 \| 3,264 B | 808 \| 816 B | `38 \| 5 B` |

### 1-byte columns / 4-bit quantization

| Chest size | Columns per chest | Chests per block (R \| C) | Chest bytes per block (R \| C) | Quantization code bytes per block (R \| C) | Unused payload per block (R \| C) |
|---:|---:|---:|---:|---:|---:|
| 4 B | 4 | 679 \| 680 | 2,716 \| 2,720 B | 1,358 \| 1,360 B | `4 \| 5 B` |
| 8 B | 8 | 339 \| 340 | 2,712 \| 2,720 B | 1,356 \| 1,360 B | `10 \| 5 B` |
| 12 B | 12 | 226 \| 226 | 2,712 \| 2,712 B | 1,356 \| 1,356 B | `10 \| 17 B` |
| 16 B | 16 | 169 \| 170 | 2,704 \| 2,720 B | 1,352 \| 1,360 B | `22 \| 5 B` |
| 20 B | 20 | 135 \| 136 | 2,700 \| 2,720 B | 1,350 \| 1,360 B | `28 \| 5 B` |
| 24 B | 24 | 113 \| 113 | 2,712 \| 2,712 B | 1,356 \| 1,356 B | `10 \| 17 B` |
| 28 B | 28 | 97 \| 97 | 2,716 \| 2,716 B | 1,358 \| 1,358 B | `4 \| 11 B` |
| 32 B | 32 | 84 \| 85 | 2,688 \| 2,720 B | 1,344 \| 1,360 B | `46 \| 5 B` |

### 2-byte columns / 2-bit quantization

| Chest size | Columns per chest | Chests per block (R \| C) | Chest bytes per block (R \| C) | Quantization code bytes per block (R \| C) | Unused payload per block (R \| C) |
|---:|---:|---:|---:|---:|---:|
| 4 B | 2 | 906 \| 907 | 3,624 \| 3,628 B | 453 \| 454 B | `1 \| 3 B` |
| 8 B | 4 | 453 \| 453 | 3,624 \| 3,624 B | 453 \| 453 B | `1 \| 8 B` |
| 12 B | 6 | 302 \| 302 | 3,624 \| 3,624 B | 453 \| 453 B | `1 \| 8 B` |
| 16 B | 8 | 226 \| 226 | 3,616 \| 3,616 B | 452 \| 452 B | `10 \| 17 B` |
| 20 B | 10 | 181 \| 181 | 3,620 \| 3,620 B | 453 \| 453 B | `5 \| 12 B` |
| 24 B | 12 | 151 \| 151 | 3,624 \| 3,624 B | 453 \| 453 B | `1 \| 8 B` |
| 28 B | 14 | 129 \| 129 | 3,612 \| 3,612 B | 452 \| 452 B | `14 \| 21 B` |
| 32 B | 16 | 113 \| 113 | 3,616 \| 3,616 B | 452 \| 452 B | `10 \| 17 B` |

### 2-byte columns / 4-bit quantization

| Chest size | Columns per chest | Chests per block (R \| C) | Chest bytes per block (R \| C) | Quantization code bytes per block (R \| C) | Unused payload per block (R \| C) |
|---:|---:|---:|---:|---:|---:|
| 4 B | 2 | 815 \| 817 | 3,260 \| 3,268 B | 815 \| 817 B | `3 \| 0 B` |
| 8 B | 4 | 407 \| 408 | 3,256 \| 3,264 B | 814 \| 816 B | `8 \| 5 B` |
| 12 B | 6 | 271 \| 272 | 3,252 \| 3,264 B | 813 \| 816 B | `13 \| 5 B` |
| 16 B | 8 | 203 \| 204 | 3,248 \| 3,264 B | 812 \| 816 B | `18 \| 5 B` |
| 20 B | 10 | 163 \| 163 | 3,260 \| 3,260 B | 815 \| 815 B | `3 \| 10 B` |
| 24 B | 12 | 135 \| 136 | 3,240 \| 3,264 B | 810 \| 816 B | `28 \| 5 B` |
| 28 B | 14 | 116 \| 116 | 3,248 \| 3,248 B | 812 \| 812 B | `18 \| 25 B` |
| 32 B | 16 | 101 \| 102 | 3,232 \| 3,264 B | 808 \| 816 B | `38 \| 5 B` |

### 4-byte columns / 2-bit quantization

| Chest size | Columns per chest | Chests per block (R \| C) | Chest bytes per block (R \| C) | Quantization code bytes per block (R \| C) | Unused payload per block (R \| C) |
|---:|---:|---:|---:|---:|---:|
| 4 B | 1 | 959 \| 961 | 3,836 \| 3,844 B | 240 \| 241 B | `2 \| 0 B` |
| 8 B | 2 | 479 \| 480 | 3,832 \| 3,840 B | 240 \| 240 B | `6 \| 5 B` |
| 12 B | 3 | 319 \| 320 | 3,828 \| 3,840 B | 240 \| 240 B | `10 \| 5 B` |
| 16 B | 4 | 239 \| 240 | 3,824 \| 3,840 B | 239 \| 240 B | `15 \| 5 B` |
| 20 B | 5 | 191 \| 192 | 3,820 \| 3,840 B | 239 \| 240 B | `19 \| 5 B` |
| 24 B | 6 | 159 \| 160 | 3,816 \| 3,840 B | 239 \| 240 B | `23 \| 5 B` |
| 28 B | 7 | 137 \| 137 | 3,836 \| 3,836 B | 240 \| 240 B | `2 \| 9 B` |
| 32 B | 8 | 119 \| 120 | 3,808 \| 3,840 B | 238 \| 240 B | `32 \| 5 B` |

### 4-byte columns / 4-bit quantization

| Chest size | Columns per chest | Chests per block (R \| C) | Chest bytes per block (R \| C) | Quantization code bytes per block (R \| C) | Unused payload per block (R \| C) |
|---:|---:|---:|---:|---:|---:|
| 4 B | 1 | 906 \| 907 | 3,624 \| 3,628 B | 453 \| 454 B | `1 \| 3 B` |
| 8 B | 2 | 453 \| 453 | 3,624 \| 3,624 B | 453 \| 453 B | `1 \| 8 B` |
| 12 B | 3 | 302 \| 302 | 3,624 \| 3,624 B | 453 \| 453 B | `1 \| 8 B` |
| 16 B | 4 | 226 \| 226 | 3,616 \| 3,616 B | 452 \| 452 B | `10 \| 17 B` |
| 20 B | 5 | 181 \| 181 | 3,620 \| 3,620 B | 453 \| 453 B | `5 \| 12 B` |
| 24 B | 6 | 151 \| 151 | 3,624 \| 3,624 B | 453 \| 453 B | `1 \| 8 B` |
| 28 B | 7 | 129 \| 129 | 3,612 \| 3,612 B | 452 \| 452 B | `14 \| 21 B` |
| 32 B | 8 | 113 \| 113 | 3,616 \| 3,616 B | 452 \| 452 B | `10 \| 17 B` |
