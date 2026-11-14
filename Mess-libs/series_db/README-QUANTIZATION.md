# Quantization Capacity

Capacity estimates assume 4,094 bytes per block, a 16-byte Record header, a 9-byte Continuation header, and one quantization code per column. Reading sizes are the sum of column widths. Unused payload space includes whole bytes and any unused bits in the final packed code byte.

## 2-bit Quantization Capacity

### 1-byte columns

| Reading size | Columns per reading | Record block readings | Reading bytes in Record block | Unused payload space | Continuation block readings | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 4 | 815 | 3,260 | 3 bytes | 817 | 0 bytes |
| 8 bytes | 8 | 407 | 3,256 | 8 bytes | 408 | 5 bytes |
| 12 bytes | 12 | 271 | 3,252 | 13 bytes | 272 | 5 bytes |
| 16 bytes | 16 | 203 | 3,248 | 18 bytes | 204 | 5 bytes |
| 20 bytes | 20 | 163 | 3,260 | 3 bytes | 163 | 10 bytes |
| 24 bytes | 24 | 135 | 3,240 | 28 bytes | 136 | 5 bytes |

### 2-byte columns

| Reading size | Columns per reading | Record block readings | Reading bytes in Record block | Unused payload space | Continuation block readings | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 2 | 906 | 3,624 | 1 byte | 907 | 3 bytes + 4 bits |
| 8 bytes | 4 | 453 | 3,624 | 1 byte | 453 | 8 bytes |
| 12 bytes | 6 | 302 | 3,624 | 1 byte | 302 | 8 bytes |
| 16 bytes | 8 | 226 | 3,616 | 10 bytes | 226 | 17 bytes |
| 20 bytes | 10 | 181 | 3,620 | 5 bytes + 4 bits | 181 | 12 bytes + 4 bits |
| 24 bytes | 12 | 151 | 3,624 | 1 byte | 151 | 8 bytes |

### 4-byte columns

| Reading size | Columns per reading | Record block readings | Reading bytes in Record block | Unused payload space | Continuation block readings | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 1 | 959 | 3,836 | 2 bytes + 2 bits | 961 | 6 bits |
| 8 bytes | 2 | 479 | 3,832 | 6 bytes + 4 bits | 480 | 5 bytes |
| 12 bytes | 3 | 319 | 3,828 | 10 bytes + 6 bits | 320 | 5 bytes |
| 16 bytes | 4 | 239 | 3,824 | 15 bytes | 240 | 5 bytes |
| 20 bytes | 5 | 191 | 3,820 | 19 bytes + 2 bits | 192 | 5 bytes |
| 24 bytes | 6 | 159 | 3,816 | 23 bytes + 4 bits | 160 | 5 bytes |

## 4-bit Quantization Capacity

### 1-byte columns

| Reading size | Columns per reading | Record block readings | Reading bytes in Record block | Unused payload space | Continuation block readings | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 4 | 679 | 2,716 | 4 bytes | 680 | 5 bytes |
| 8 bytes | 8 | 339 | 2,712 | 10 bytes | 340 | 5 bytes |
| 12 bytes | 12 | 226 | 2,712 | 10 bytes | 226 | 17 bytes |
| 16 bytes | 16 | 169 | 2,704 | 22 bytes | 170 | 5 bytes |
| 20 bytes | 20 | 135 | 2,700 | 28 bytes | 136 | 5 bytes |
| 24 bytes | 24 | 113 | 2,712 | 10 bytes | 113 | 17 bytes |

### 2-byte columns

| Reading size | Columns per reading | Record block readings | Reading bytes in Record block | Unused payload space | Continuation block readings | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 2 | 815 | 3,260 | 3 bytes | 817 | 0 bytes |
| 8 bytes | 4 | 407 | 3,256 | 8 bytes | 408 | 5 bytes |
| 12 bytes | 6 | 271 | 3,252 | 13 bytes | 272 | 5 bytes |
| 16 bytes | 8 | 203 | 3,248 | 18 bytes | 204 | 5 bytes |
| 20 bytes | 10 | 163 | 3,260 | 3 bytes | 163 | 10 bytes |
| 24 bytes | 12 | 135 | 3,240 | 28 bytes | 136 | 5 bytes |

### 4-byte columns

| Reading size | Columns per reading | Record block readings | Reading bytes in Record block | Unused payload space | Continuation block readings | Unused payload space |
|---:|---:|---:|---:|---:|---:|---:|
| 4 bytes | 1 | 906 | 3,624 | 1 byte | 907 | 3 bytes + 4 bits |
| 8 bytes | 2 | 453 | 3,624 | 1 byte | 453 | 8 bytes |
| 12 bytes | 3 | 302 | 3,624 | 1 byte | 302 | 8 bytes |
| 16 bytes | 4 | 226 | 3,616 | 10 bytes | 226 | 17 bytes |
| 20 bytes | 5 | 181 | 3,620 | 5 bytes + 4 bits | 181 | 12 bytes + 4 bits |
| 24 bytes | 6 | 151 | 3,624 | 1 byte | 151 | 8 bytes |
