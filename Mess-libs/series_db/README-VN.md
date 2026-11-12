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

> Trong danh sách trường:
>
> - `*` nghĩa là trường dự kiến sẽ duy trì lâu dài trong định dạng.
> - `**` nghĩa là kích thước trường có thể cấu hình.

## Superblock (S0, S1)

Hai khối cố định dùng để cấu hình cơ sở dữ liệu là các bản sao của nhau. Nếu một khối bị hỏng, chẳng hạn **`magic number`** hoặc **`CRC`** không khớp, có thể dùng khối còn lại để khôi phục cấu hình.

Superblock cũng cần một số `version`. Số này xác định định dạng dự kiến và cho phép thay đổi định dạng theo thời gian, chẳng hạn từ phiên bản 1 sang phiên bản 2.

Mỗi Superblock gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **config** (`G`) | 1 byte<sup>*</sup> | Lưu các tùy chọn cấu hình chung của cơ sở dữ liệu. |
| **version** (`V`) | 1 byte<sup>*</sup> | Xác định định dạng Superblock. |
| **magic_number** (`M`) | 2 byte<sup>*</sup> | Xác định điểm bắt đầu của header Superblock. |
| **flags** (`F`) | 1 byte | Lưu các cờ của Superblock. |
| **crc16** (`C`) | 4 byte<sup>**</sup> | Bao gồm `config`, `version`, `magic_number` và `flags`; không bao gồm chính trường CRC hoặc `schema_entries`. |
| **schema_entries** (`S`) | Phần còn lại | Các mục schema chỉ ghi nối tiếp. Mỗi mục có CRC và dấu đánh dấu đã ghi hoàn tất. |

Superblock được lưu thành hai bản sao S0 và S1. Mỗi bản sao có bố cục trường như sau:

```text
       +---+---+---+---+---+---+
S0:    | G | V | M | F | C | S |
       +---+---+---+---+---+---+
S1:    | G | V | M | F | C | S |
       +---+---+---+---+---+---+
```

Byte `config` của Superblock dùng các bit như sau:

| Bit | Ý nghĩa |
|---|---|
| 7–4 | Dành riêng; ghi bằng `0`. |
| 3–0 | Dành riêng; ghi bằng `0`. |

Mục schema lưu bố cục payload mà record block tham chiếu đến. `schema_config` cấu hình đơn vị độ rộng cột, thứ tự byte và việc có các trường tùy chọn `col_units`, các trường scale hoặc bitset `col_signs` hay không. Bit 7 được dành cho khả năng mở rộng trong tương lai. Xem chi tiết trong [README-CONSTRAINTS.md](README-CONSTRAINTS.md#schema-config).

> Dữ liệu mới dùng schema ID mới và **phải được ghi vào một khối mới**. Thay đổi schema là ranh giới cứng: extent cũ kết thúc và extent mới bắt đầu. Vì một extent bắt đầu bằng một record block, việc bắt đầu extent luôn cần ghi ít nhất một khối mới.

## Record block (R)

Các record block (`R0`–`Rn`) nằm sau hai Superblock `S0` và `S1`. Mỗi record block thuộc một luồng độ phân giải được xác định bằng `stream_id` và dùng một `schema_id` có mục tương ứng đã được xác thực trong cả hai Superblock. Các luồng dùng chung một vùng block khả dụng.

Một record block gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte<sup>*</sup> | Xác định điểm bắt đầu của record block. |
| **crc16** | 2 byte<sup>(*)</sup> | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu trong khối bị hỏng. |
| **schema_id** | 1 byte<sup>(*)</sup> | Xác định schema dùng để diễn giải payload. |
| **stream_id** | 1 byte<sup>*</sup> | Xác định luồng độ phân giải mà block này thuộc về. |
| **stream_sequence** | 4 byte<sup>*</sup> | Xác định thứ tự block trong luồng. |
| **record_config** | 6 byte<sup>*</sup> | Lưu thời gian, định dạng và các cờ cho bản ghi. |
| **payload** | Thay đổi | Lưu các bản ghi. |

## Continuation block (C)

Continuation block lưu các bản ghi tiếp nối record block `R` khi payload của nó đã đầy. Mỗi block có cùng `stream_id` và số thứ tự block trong luồng để xác định extent tiếp nối, ngay cả khi block của các luồng khác nằm xen kẽ trong flash. Khi một continuation block `C` đầy, một continuation block khác `C_cont` sẽ được thêm vào. Xem quy tắc thứ tự và khôi phục trong [README-CONSTRAINTS.md](README-CONSTRAINTS.md#continuation-block-association-and-order).

Một continuation block gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte<sup>*</sup> | Xác định điểm bắt đầu của continuation block. |
| **crc16** | 2 byte<sup>(*)</sup> | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu trong khối bị hỏng. |
| **stream_id** | 1 byte<sup>*</sup> | Xác định luồng độ phân giải mà block này thuộc về. |
| **stream_sequence** | 4 byte<sup>*</sup> | Xác định thứ tự block trong luồng. |
| **payload** | Thay đổi | Lưu các bản ghi tiếp nối từ khối trước đó. |

Các continuation block có thể nằm xen kẽ với block của luồng khác; `stream_sequence` xác định thứ tự logic của chúng trong từng luồng.

```text
[S0] [S1]
[stream 0: R0] [stream 1: R0] [stream 0: C1] [stream 2: R0] ...
...
```

> Mỗi cặp ngoặc vuông biểu thị một khối 4 KiB.
