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

Mục schema lưu bố cục payload mà record block tham chiếu đến. Mỗi schema hỗ trợ từ 1 đến 16 cột. `col_widths` xác định số byte dành cho từng cột trong vùng giá trị đọc; mỗi cột có một mã `col_units`. Mã `3` chỉ các bit đóng gói: 1 byte dữ liệu chứa 8 giá trị bật/tắt. Khi bit 0 của `schema_config2` được bật, mảng `col_quant_bits` riêng lưu số bit của mã lượng tử hóa cho từng cột. `schema_config1` cấu hình thứ tự byte, các trường tùy chọn và việc áp dụng dấu của cột; `col_signs` luôn có mặt. Xem chi tiết trong [README-CONSTRAINTS.md](README-CONSTRAINTS.md#schema_entries).

> Dữ liệu mới dùng schema ID mới và **phải được ghi vào một khối mới**. Thay đổi schema là ranh giới cứng: extent cũ kết thúc và extent mới bắt đầu. Vì một extent bắt đầu bằng một record block, việc bắt đầu extent luôn cần ghi ít nhất một khối mới.

## Record block (R)

Các record block (`R0`–`Rn`) nằm sau hai Superblock `S0` và `S1`. Mỗi record block thuộc một luồng độ phân giải được xác định bằng `stream_id` và dùng một `schema_id` có mục tương ứng đã được xác thực trong cả hai Superblock. Các luồng dùng chung một vùng block khả dụng.

Một record block gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte<sup>*</sup> | Xác định điểm bắt đầu của record block. |
| **crc16** | 2 byte<sup>(*)</sup> | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu trong khối bị hỏng. |
| **schema_id** | 1 byte<sup>(*)</sup> | Xác định schema chứa `col_widths` và `col_quant_bits` tùy chọn của block này. |
| **stream_id** | 1 byte<sup>*</sup> | Xác định luồng độ phân giải mà block này thuộc về. |
| **stream_sequence** | 4 byte<sup>*</sup> | Xác định thứ tự block trong luồng. |
| **record_config** | 6 byte<sup>*</sup> | Lưu thời gian, định dạng và các cờ cho bản ghi. |
| **payload** | Thay đổi | Lưu vùng giá trị đọc và vùng mã lượng tử hóa được đóng gói khi bật tính năng này. |

`col_quant_bits` được lưu một lần trong mục schema và chọn số bit của mã lượng tử hóa cho từng cột. Các mã nằm trong payload của block.

## Continuation block (C)

Continuation block lưu các bản ghi tiếp nối record block `R` khi payload của nó đã đầy. Mỗi block có cùng `stream_id` và số thứ tự block trong luồng để xác định extent tiếp nối, ngay cả khi block của các luồng khác nằm xen kẽ trong flash. Khi một continuation block `C` đầy, một continuation block khác `C_cont` sẽ được thêm vào. Xem quy tắc thứ tự và khôi phục trong [README-CONSTRAINTS.md](README-CONSTRAINTS.md#continuation-block-association-and-order).

Một continuation block gồm:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte<sup>*</sup> | Xác định điểm bắt đầu của continuation block. |
| **crc16** | 2 byte<sup>(*)</sup> | Phát hiện các thay đổi ngoài ý muốn hoặc dữ liệu trong khối bị hỏng. |
| **stream_id** | 1 byte<sup>*</sup> | Xác định luồng độ phân giải mà block này thuộc về. |
| **stream_sequence** | 4 byte<sup>*</sup> | Xác định thứ tự block trong luồng. |
| **payload** | Thay đổi | Lưu các giá trị đọc tiếp nối và mã lượng tử hóa theo schema của Record block trong cùng extent. |

Các continuation block có thể nằm xen kẽ với block của luồng khác; `stream_sequence` xác định thứ tự logic của chúng trong từng luồng.

## Payload

Mỗi Record block hoặc Continuation block có 2 vùng trong payload của chính nó:

```text
[header của block] [vùng giá trị đọc] [vùng mã lượng tử hóa được đóng gói]
```

`col_widths` xác định kích thước các cột trong vùng giá trị đọc. Khi bật `col_quant_bits`, mỗi giá trị khác `0` xác định số bit mã lượng tử hóa cho cột đó trong từng bản ghi (`1`, `2` hoặc `4` bit). Các mã được đóng gói theo thứ tự bản ghi: mã số 0 thuộc bản ghi số 0. Mỗi Continuation block có vùng mã riêng cho các bản ghi của nó. Mục schema lưu số bit; payload lưu các mã.

Ví dụ, `col_widths = [1]` và `col_units = [3]` tạo một cột chứa 8 giá trị bật/tắt trong mỗi bản ghi. Byte `01010011` biểu thị các giá trị `0, 1, 0, 1, 0, 0, 1, 1` theo thứ tự từ bit 7 đến bit 0. Các bit này thuộc vùng giá trị đọc, không thuộc vùng mã lượng tử hóa.

Kích thước 2 vùng phải được dành trước khi ghi block để vùng mã không bị dịch chuyển khi thêm bản ghi. Byte mã cuối có thể còn bit chưa dùng. Định dạng block vẫn cần số bản ghi và ranh giới vùng mã để người đọc tìm đúng 2 vùng.

```text
[S0] [S1]
[stream 0: R0] [stream 1: R0] [stream 0: C1] [stream 2: R0] ...
...
```

> Mỗi cặp ngoặc vuông biểu thị một khối 4 KiB.
