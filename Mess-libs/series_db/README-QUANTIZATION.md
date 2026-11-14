# Quantization Capacity

Capacity estimates assume 4,094 bytes per block, a 16-byte Record header, a 9-byte Continuation header, and one quantization code per column. A **chest** is the stored column values for one record; its size is the sum of the column widths and excludes quantization codes. Unused payload space includes whole bytes and any unused bits in the final packed code byte.

Let `Payload` be the payload capacity (`4,078` bytes for a Record block; `4,085` for a Continuation block), `Chest_size` the chest size in bytes, `Q` the total quantization code bits per chest, and `N` the maximum whole chests that fit:

```text
N = largest integer such that N × Chest_size + ceil(N × Q / 8) ≤ Payload
unused whole bytes = Payload − N × Chest_size − ceil(N × Q / 8)
```

In these tables, columns scale with chest size and each column gets one code. Therefore, total chest bytes per block change mainly with quantization code width, not chest size; small differences are due to packing and rounding.

Switching from 4-bit to 2-bit codes halves the code area. For a 4-byte chest with four 1-byte columns, a Record block holds **815 instead of 679 records**. With one 4-byte column, it holds **959 instead of 906 records**. Fewer, wider columns need fewer codes.

## 2-bit Quantization Capacity

### 1-byte columns

| Chest size (bytes) | Columns per chest | Chest bytes in Record block | Chests per Record block | Unused payload space | Chests per Continuation block | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 4 | 3,260 | 815 | 3 bytes | 817 | 0 bytes |
| 8 bytes | 8 | 3,256 | 407 | 8 bytes | 408 | 5 bytes |
| 12 bytes | 12 | 3,252 | 271 | 13 bytes | 272 | 5 bytes |
| 16 bytes | 16 | 3,248 | 203 | 18 bytes | 204 | 5 bytes |
| 20 bytes | 20 | 3,260 | 163 | 3 bytes | 163 | 10 bytes |
| 24 bytes | 24 | 3,240 | 135 | 28 bytes | 136 | 5 bytes |

### 2-byte columns

| Chest size (bytes) | Columns per chest | Chest bytes in Record block | Chests per Record block | Unused payload space | Chests per Continuation block | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 2 | 3,624 | 906 | 1 byte | 907 | 3 bytes + 4 bits |
| 8 bytes | 4 | 3,624 | 453 | 1 byte | 453 | 8 bytes |
| 12 bytes | 6 | 3,624 | 302 | 1 byte | 302 | 8 bytes |
| 16 bytes | 8 | 3,616 | 226 | 10 bytes | 226 | 17 bytes |
| 20 bytes | 10 | 3,620 | 181 | 5 bytes + 4 bits | 181 | 12 bytes + 4 bits |
| 24 bytes | 12 | 3,624 | 151 | 1 byte | 151 | 8 bytes |

### 4-byte columns

| Chest size (bytes) | Columns per chest | Chest bytes in Record block | Chests per Record block | Unused payload space | Chests per Continuation block | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 1 | 3,836 | 959 | 2 bytes + 2 bits | 961 | 6 bits |
| 8 bytes | 2 | 3,832 | 479 | 6 bytes + 4 bits | 480 | 5 bytes |
| 12 bytes | 3 | 3,828 | 319 | 10 bytes + 6 bits | 320 | 5 bytes |
| 16 bytes | 4 | 3,824 | 239 | 15 bytes | 240 | 5 bytes |
| 20 bytes | 5 | 3,820 | 191 | 19 bytes + 2 bits | 192 | 5 bytes |
| 24 bytes | 6 | 3,816 | 159 | 23 bytes + 4 bits | 160 | 5 bytes |

## 4-bit Quantization Capacity

### 1-byte columns

| Chest size (bytes) | Columns per chest | Chest bytes in Record block | Chests per Record block | Unused payload space | Chests per Continuation block | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 4 | 2,716 | 679 | 4 bytes | 680 | 5 bytes |
| 8 bytes | 8 | 2,712 | 339 | 10 bytes | 340 | 5 bytes |
| 12 bytes | 12 | 2,712 | 226 | 10 bytes | 226 | 17 bytes |
| 16 bytes | 16 | 2,704 | 169 | 22 bytes | 170 | 5 bytes |
| 20 bytes | 20 | 2,700 | 135 | 28 bytes | 136 | 5 bytes |
| 24 bytes | 24 | 2,712 | 113 | 10 bytes | 113 | 17 bytes |

### 2-byte columns

| Chest size (bytes) | Columns per chest | Chest bytes in Record block | Chests per Record block | Unused payload space | Chests per Continuation block | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 2 | 3,260 | 815 | 3 bytes | 817 | 0 bytes |
| 8 bytes | 4 | 3,256 | 407 | 8 bytes | 408 | 5 bytes |
| 12 bytes | 6 | 3,252 | 271 | 13 bytes | 272 | 5 bytes |
| 16 bytes | 8 | 3,248 | 203 | 18 bytes | 204 | 5 bytes |
| 20 bytes | 10 | 3,260 | 163 | 3 bytes | 163 | 10 bytes |
| 24 bytes | 12 | 3,240 | 135 | 28 bytes | 136 | 5 bytes |

### 4-byte columns

| Chest size (bytes) | Columns per chest | Chest bytes in Record block | Chests per Record block | Unused payload space | Chests per Continuation block | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 1 | 3,624 | 906 | 1 byte | 907 | 3 bytes + 4 bits |
| 8 bytes | 2 | 3,624 | 453 | 1 byte | 453 | 8 bytes |
| 12 bytes | 3 | 3,624 | 302 | 1 byte | 302 | 8 bytes |
| 16 bytes | 4 | 3,616 | 226 | 10 bytes | 226 | 17 bytes |
| 20 bytes | 5 | 3,620 | 181 | 5 bytes + 4 bits | 181 | 12 bytes + 4 bits |
| 24 bytes | 6 | 3,624 | 151 | 1 byte | 151 | 8 bytes |
