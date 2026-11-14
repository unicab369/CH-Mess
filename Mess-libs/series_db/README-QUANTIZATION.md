# Quantization Capacity

Capacity estimates assume 4,094 bytes per block, a 16-byte Record header, a 9-byte Continuation header, and one quantization code per column. A **chest** is the stored column values for one record; its size is the sum of the column widths and excludes quantization codes. Unused payload space is shown in whole bytes; leftover bits are omitted.

Let `Payload` be the payload capacity (`4,078` bytes for a Record block; `4,085` for a Continuation block), `Chest_size` the chest size in bytes, `Q` the total quantization code bits per chest, and `N` the maximum whole chests that fit:

```text
N = largest integer such that N × Chest_size + ceil(N × Q / 8) ≤ Payload
unused whole bytes = Payload − N × Chest_size − ceil(N × Q / 8)
```

In these tables, columns scale with chest size and each column gets one code. Therefore, total chest bytes per block change mainly with quantization code width, not chest size; small differences are due to packing and rounding.

Switching from 4-bit to 2-bit codes halves the code area. For a 4-byte chest with four 1-byte columns, a Record block holds **815 instead of 679 records**. With one 4-byte column, it holds **959 instead of 906 records**. Fewer, wider columns need fewer codes.


## Quantization Capacity by Column Width

B = byte. Each capacity cell lists the 2-bit result first and the 4-bit result second.


### 1-byte columns

| Chest size | Columns per chest | Total chest bytes in Record block (B) | Chests per Record block | Unused Record payload | Chests per Continuation block | **Unused Continuation payload** |
|---:|---:|---:|---:|---:|---:|---:|
| 4 B | 4 | 2-bit: 3,260<br>4-bit: 2,716 | 2-bit: 815<br>4-bit: 679 | **2-bit: 3 B<br>4-bit: 4 B** | 2-bit: 817<br>4-bit: 680 | **2-bit: 0 B<br>4-bit: 5 B** |
| 8 B | 8 | 2-bit: 3,256<br>4-bit: 2,712 | 2-bit: 407<br>4-bit: 339 | **2-bit: 8 B<br>4-bit: 10 B** | 2-bit: 408<br>4-bit: 340 | **2-bit: 5 B<br>4-bit: 5 B** |
| 12 B | 12 | 2-bit: 3,252<br>4-bit: 2,712 | 2-bit: 271<br>4-bit: 226 | **2-bit: 13 B<br>4-bit: 10 B** | 2-bit: 272<br>4-bit: 226 | **2-bit: 5 B<br>4-bit: 17 B** |
| 16 B | 16 | 2-bit: 3,248<br>4-bit: 2,704 | 2-bit: 203<br>4-bit: 169 | **2-bit: 18 B<br>4-bit: 22 B** | 2-bit: 204<br>4-bit: 170 | **2-bit: 5 B<br>4-bit: 5 B** |
| 20 B | 20 | 2-bit: 3,260<br>4-bit: 2,700 | 2-bit: 163<br>4-bit: 135 | **2-bit: 3 B<br>4-bit: 28 B** | 2-bit: 163<br>4-bit: 136 | **2-bit: 10 B<br>4-bit: 5 B** |
| 24 B | 24 | 2-bit: 3,240<br>4-bit: 2,712 | 2-bit: 135<br>4-bit: 113 | **2-bit: 28 B<br>4-bit: 10 B** | 2-bit: 136<br>4-bit: 113 | **2-bit: 5 B<br>4-bit: 17 B** |

### 2-byte columns

| Chest size | Columns per chest | Total chest bytes in Record block (B) | Chests per Record block | Unused Record payload | Chests per Continuation block | **Unused Continuation payload** |
|---:|---:|---:|---:|---:|---:|---:|
| 4 B | 2 | 2-bit: 3,624<br>4-bit: 3,260 | 2-bit: 906<br>4-bit: 815 | **2-bit: 1 B<br>4-bit: 3 B** | 2-bit: 907<br>4-bit: 817 | **2-bit: 3 B<br>4-bit: 0 B** |
| 8 B | 4 | 2-bit: 3,624<br>4-bit: 3,256 | 2-bit: 453<br>4-bit: 407 | **2-bit: 1 B<br>4-bit: 8 B** | 2-bit: 453<br>4-bit: 408 | **2-bit: 8 B<br>4-bit: 5 B** |
| 12 B | 6 | 2-bit: 3,624<br>4-bit: 3,252 | 2-bit: 302<br>4-bit: 271 | **2-bit: 1 B<br>4-bit: 13 B** | 2-bit: 302<br>4-bit: 272 | **2-bit: 8 B<br>4-bit: 5 B** |
| 16 B | 8 | 2-bit: 3,616<br>4-bit: 3,248 | 2-bit: 226<br>4-bit: 203 | **2-bit: 10 B<br>4-bit: 18 B** | 2-bit: 226<br>4-bit: 204 | **2-bit: 17 B<br>4-bit: 5 B** |
| 20 B | 10 | 2-bit: 3,620<br>4-bit: 3,260 | 2-bit: 181<br>4-bit: 163 | **2-bit: 5 B<br>4-bit: 3 B** | 2-bit: 181<br>4-bit: 163 | **2-bit: 12 B<br>4-bit: 10 B** |
| 24 B | 12 | 2-bit: 3,624<br>4-bit: 3,240 | 2-bit: 151<br>4-bit: 135 | **2-bit: 1 B<br>4-bit: 28 B** | 2-bit: 151<br>4-bit: 136 | **2-bit: 8 B<br>4-bit: 5 B** |

### 4-byte columns

| Chest size | Columns per chest | Total chest bytes in Record block (B) | Chests per Record block | Unused Record payload | Chests per Continuation block | **Unused Continuation payload** |
|---:|---:|---:|---:|---:|---:|---:|
| 4 B | 1 | 2-bit: 3,836<br>4-bit: 3,624 | 2-bit: 959<br>4-bit: 906 | **2-bit: 2 B<br>4-bit: 1 B** | 2-bit: 961<br>4-bit: 907 | **2-bit: 0 B<br>4-bit: 3 B** |
| 8 B | 2 | 2-bit: 3,832<br>4-bit: 3,624 | 2-bit: 479<br>4-bit: 453 | **2-bit: 6 B<br>4-bit: 1 B** | 2-bit: 480<br>4-bit: 453 | **2-bit: 5 B<br>4-bit: 8 B** |
| 12 B | 3 | 2-bit: 3,828<br>4-bit: 3,624 | 2-bit: 319<br>4-bit: 302 | **2-bit: 10 B<br>4-bit: 1 B** | 2-bit: 320<br>4-bit: 302 | **2-bit: 5 B<br>4-bit: 8 B** |
| 16 B | 4 | 2-bit: 3,824<br>4-bit: 3,616 | 2-bit: 239<br>4-bit: 226 | **2-bit: 15 B<br>4-bit: 10 B** | 2-bit: 240<br>4-bit: 226 | **2-bit: 5 B<br>4-bit: 17 B** |
| 20 B | 5 | 2-bit: 3,820<br>4-bit: 3,620 | 2-bit: 191<br>4-bit: 181 | **2-bit: 19 B<br>4-bit: 5 B** | 2-bit: 192<br>4-bit: 181 | **2-bit: 5 B<br>4-bit: 12 B** |
| 24 B | 6 | 2-bit: 3,816<br>4-bit: 3,624 | 2-bit: 159<br>4-bit: 151 | **2-bit: 23 B<br>4-bit: 1 B** | 2-bit: 160<br>4-bit: 151 | **2-bit: 5 B<br>4-bit: 8 B** |
