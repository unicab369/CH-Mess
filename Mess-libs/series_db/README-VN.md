# Series DB

Bộ nhớ flash được chia thành các khối. Mỗi khối có thể lưu trữ 4094 byte dữ liệu.

Khi truy cập cơ sở dữ liệu, cần ít nhất hai thứ:

- **Magic number:** Xác định điểm bắt đầu của một header và cho biết thông tin của header đó bắt đầu tại địa chỉ này.
- **CRC checksum:** Phát hiện những thay đổi ngoài ý muốn hoặc dữ liệu thô bị hỏng. Một phép toán rút gọn mỗi tập dữ liệu—ví dụ 250 byte—thành một checksum nhỏ hơn. So sánh checksum của hai tập dữ liệu để kiểm tra chúng có giống nhau hay không, **`mà không cần so sánh lần lượt cả 250 byte`**:

  ```text
  Tập 1: 250 byte dữ liệu → checksum 2 byte 1
  Tập 2: 250 byte dữ liệu → checksum 2 byte 2
  ```

  Nếu checksum trùng nhau, dữ liệu có khả năng giống nhau; nếu khác nhau, dữ liệu đã thay đổi hoặc bị hỏng.

## Superblock (B0, B1)

Hai khối cố định dùng để cấu hình cơ sở dữ liệu là các bản sao của nhau. Nếu một khối bị hỏng, chẳng hạn **`magic number`** hoặc **`CRC`** không khớp, có thể dùng khối còn lại để khôi phục cấu hình.

Superblock cũng cần một số `version`. Số này xác định định dạng dự kiến và cho phép thay đổi định dạng theo thời gian, chẳng hạn từ phiên bản 1 sang phiên bản 2.

Mỗi superblock gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number\*** (`M`) | 2 byte | Xác định điểm bắt đầu của header superblock. |
| **crc_16**<sup>(*)</sup> (`C`) | 2 byte | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu superblock bị hỏng. |
| **version\*** (`V`) | 1 byte | Xác định định dạng superblock. |
| **flags** (`F`) | 1 byte | Lưu các cờ của superblock. |
| **schema_table** (`S`) | Thay đổi | Ánh xạ schema ID sang bố cục payload. |

> Trong danh sách trường, `*` nghĩa là trường bắt buộc. `(*)` nghĩa là trường bắt buộc nhưng kích thước vẫn cần được xác định.

Superblock được lưu thành hai bản sao B0 và B1. Mỗi bản sao có cùng các trường:

```text
[B0: M, V, C, F, S]
[B1: M, V, C, F, S]
```

## Bảng schema (S)

Bảng schema nằm trong superblock. Khi đọc dữ liệu, bảng trả lời câu hỏi: “Với một `schema_id`, bố cục byte của payload là gì?”

Ví dụ, giả sử payload có 4 byte và schema quy định 2 byte đầu là nhiệt độ, 2 byte tiếp theo là độ ẩm:

```text
payload     = 0x01 0x02 0x03 0x04
temperature = 0x01 0x02
humidity    = 0x03 0x04
```

Bảng schema gồm một header bảng, theo sau là các mục schema:

```text
B0 (M, V, C, F)
└── Schema table (S)
    ├── Table header (Th)
    ├── Entry 0 (T_e0)
    ├── Entry 1 (T_e1)
    ├── Entry 2 (T_e2)
    └── ...
```

Header bảng (Th) gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **crc_16**<sup>(*)</sup> | 2 byte | Checksum được tính trên tất cả các mục schema. |
| **schema_count\*** | 1 byte | Số lượng mục schema trong bảng. |
| **table_length\*** | 1 byte | Độ dài của bảng schema. |

Một mục schema (S_en) gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **schema_id\*** | 1 byte | Định danh mục schema. |
| **column_count\*** | 1 byte | Số cột trong schema. |
| **column_widths**<sup>(*)</sup> | 8 byte | Lưu độ rộng của từng cột. |
| **column_signs**<sup>(*)</sup> | 8 bit | Mỗi cột dùng một bit, từ bit có trọng số cao nhất đến thấp nhất; `1` = có dấu, `0` = không dấu. |

Ví dụ:

```text
schema_id     = 0
column_count  = 3
column_widths = [8, 16, 32]
column_signs  = 0b01000000
```

Mục này mô tả 3 cột:

- Cột 1: giá trị 8 bit không dấu.
- Cột 2: giá trị 16 bit có dấu, có thể nhận giá trị âm.
- Cột 3: giá trị 32 bit không dấu.

Tóm lại, mỗi cơ sở dữ liệu có một bảng schema. Bảng có một header và có thể chứa nhiều mục schema.

```text
B0 (M, V, C, F)
└── schema_table (S)
    ├── table_header (Th)
    │   ├── crc_16 (2 byte)
    │   ├── schema_count (1 byte)
    │   └── table_length (1 byte)
    ├── entry_0 (T_e0)
    │   ├── schema_id (1 byte)
    │   ├── column_count (1 byte)
    │   ├── column_widths (8 byte)
    │   └── column_signs (8 bit)
    ├── entry_1 (T_e1)
    │   └── ...
    └── ...
```

Mỗi extent mang một `schema_id`. Bộ giải mã tra cứu ID này để xác định vị trí của từng cột trong payload. Khi schema thay đổi, các extent hiện có vẫn giữ ID ban đầu và có thể tiếp tục được giải mã theo bố cục cũ.

> Dữ liệu mới dùng schema ID mới và **phải được ghi vào một khối mới**. Thay đổi schema là ranh giới cứng: extent cũ kết thúc và extent mới bắt đầu. Vì một extent bắt đầu bằng một record block, việc bắt đầu extent luôn cần ghi ít nhất một khối mới.

## Record block (R)

Các record block (`R0`–`Rn`) nằm sau hai superblock `B0` và `B1`. Chúng lưu các bản ghi, chẳng hạn các số đo nhiệt độ theo chuỗi thời gian.

Một record block gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number\*** | 2 byte | Xác định điểm bắt đầu của record block. |
| **crc_16**<sup>(*)</sup> | 2 byte | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu trong khối bị hỏng. |
| **schema_id**<sup>(*)</sup> | 2 byte | Xác định schema dùng để diễn giải payload. |
| **flags**<sup>(*)</sup> | 8 bit | Lưu các cờ của record block. |
| **timing_config**<sup>(*)</sup> | 8 byte | Lưu các thiết lập thời gian dùng để diễn giải timestamp của bản ghi. |
| **payload** | Thay đổi | Lưu các bản ghi. |

## Continuation block (C)

Continuation block lưu các bản ghi tiếp nối record block `R` khi payload của nó đã đầy. Khi một continuation block `C` đầy, một continuation block khác `C_cont` sẽ được thêm vào. Chuỗi kết thúc khi một record block mới `R_new` được ghi, chẳng hạn sau khi schema thay đổi hoặc khôi phục sau khi mất điện. Continuation block tránh phải lặp lại metadata của record block trong mọi khối thuộc cùng một extent.

Một continuation block gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number\*** | 2 byte | Xác định điểm bắt đầu của continuation block. |
| **crc_16**<sup>(*)</sup> | 2 byte | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu trong khối bị hỏng. |
| **payload** | Thay đổi | Lưu các bản ghi tiếp nối từ khối trước đó. |

Sau mỗi record block là các continuation block cần thiết để lưu phần bản ghi còn lại.

```text
[B0] [B1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Mỗi cặp ngoặc vuông biểu thị một khối 4 KiB.
