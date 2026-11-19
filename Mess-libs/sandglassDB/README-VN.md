# SandglassDB

## Tổng quan

Cơ sở dữ liệu bắt đầu bằng 2 Superblock (`S0`, `S1`). Các khối còn lại thuộc một vùng lưu trữ chung. Khối Record (`R`) bắt đầu một extent; các khối Continuation (`C`) nối dài extent đó khi payload đầy. Mỗi `dataset_id` xác định một luồng; khối `R` lưu `interval_code`, còn `stream_counter` xác định thứ tự khối trong luồng. Khối của nhiều luồng có thể xen kẽ nhau.

**Chest** là tập giá trị cột của một lần đọc, nằm trong payload của khối `R` hoặc `C`. Khối ghi trực tiếp và khối lưu trữ dài hạn đều có thể chọn giá trị raw hoặc lượng tử hóa đều, cùng các bước nén không mất dữ liệu. Nhiệt độ raw có thể dùng số nguyên với hệ số thập phân cố định; xem [Archiving](README-ARCHIVE.md).

```text
[S0] [S1]
[R0] [R0-C0] [R0-C1] [R0-C2] ...
[R1] [R1-C0] [R1-C1] [R1-C2] ...
[R2] [R2-C0] [R2-C1] [R2-C2] ...
...
```

> Mỗi cặp ngoặc vuông biểu thị một khối 4 KiB.

Flash được chia thành các khối; mỗi khối có thể lưu 4.094 byte dữ liệu. Magic number đánh dấu đầu header. CRC giúp phát hiện dữ liệu bị thay đổi hoặc hỏng.

> Trong danh sách trường, `*` chỉ trường dự kiến tồn tại lâu dài; `**` chỉ kích thước trường có thể cấu hình.

## Superblock (`S0`, `S1`)

Hai Superblock là hai bản sao cấu hình. Nếu một bản bị hỏng, có thể dùng bản còn lại. `version` xác định phiên bản định dạng.

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **config** | 1 byte* | Cấu hình chung của cơ sở dữ liệu. |
| **version** | 1 byte* | Phiên bản định dạng Superblock. |
| **generation** | 1 byte* | Xác định header Superblock cố định mới hơn; quay từ `255` về `0`. |
| **magic_number** | 2 byte* | Đánh dấu đầu header. |
| **flags** | 1 byte* | Các cờ của Superblock. |
| **crc16** | 4 byte** | Bao phủ header cố định và phần đệm căn chỉnh; không bao gồm chính CRC hoặc `schema_entries`. |
| **alignment_padding** | 2 byte* | Đặt bằng `0` để căn `schema_entries` theo ranh giới 4 byte. |
| **schema_entries** | 4.084 byte | Các mục schema được ghi nối tiếp, mỗi mục có CRC và dấu xác nhận riêng. |

```text
       +--------+---------+------------+-------------+-------+-------+-------------------+---------------+
S0:    | config | version | generation | magic_number | flags | crc16 | alignment_padding | schema_entries |
       +--------+---------+------------+-------------+-------+-------+-------------------+---------------+
S1:    | config | version | generation | magic_number | flags | crc16 | alignment_padding | schema_entries |
       +--------+---------+------------+-------------+-------+-------+-------------------+---------------+
```

Các trường cố định chiếm 10 byte. `alignment_padding` 2 byte đặt `schema_entries` tại offset 12; mỗi mục schema cũng được đệm để có kích thước là bội số của 4 byte.

## Mục Schema

Định nghĩa schema được ghi nối tiếp trong `schema_entries` của cả `S0` và `S1`. Chỉ dùng `schema_id` mới sau khi mục tương ứng đã được xác nhận trong cả hai bản. CRC của phần cố định Superblock không bao phủ các mục này; mỗi mục có CRC riêng. Reader quét chúng để tạo bảng tra từ `schema_id` sang bố cục payload.

Trường áp dụng cho toàn schema:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **entry_crc** | 2 byte* | Kiểm tra mục schema. |
| **commit_marker** | 1 byte* | Đánh dấu mục đã ghi xong. |
| **schema_config1** | 1 byte* | Chọn thứ tự byte và các trường tùy chọn; xem [schema_config1](#schema_config1). |
| **schema_id** | 1 byte* | ID của mục schema. |
| **col_count** | 1 byte* | Số cột; `1`–`255` hợp lệ, `0` dành riêng. |

Trường theo cột, theo thứ tự trong mục:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **col_widths** | 1 byte / cột | Số byte cấp cho mỗi cột trong Chest. |
| **col_units** | 1 byte* / cột | Mã đơn vị của mỗi cột; xem [mã đơn vị](#mã-đơn-vị-col_units). |
| **col_scale_mul** | 1 byte** / cột | Có khi `schema_config1` bit 4 bằng `1`; hệ số nhân mặc định là `1`. Ví dụ `10 × 3 = 30`. |
| **col_scale_exp** | 1 byte có dấu** / cột | Có khi bit 2 bằng `1`; số mũ cơ số 10 mặc định là `0`. Ví dụ `123 × 10⁻² = 1,23`. |
| **col_scale_ratios** | 2 byte** / cột | Có khi bit 3 bằng `1`; cặp tử số/mẫu số mặc định là `1/1`. Ví dụ `10 × 3/2 = 15`. |
| **col_signs** | `ceil(col_count / 8)` byte* | Một bit cho mỗi cột (`1` = có dấu, `0` = không dấu); dùng khi `schema_config1` bit 0 bằng `1`. Bit thừa bằng `0`. |
| **padding** | Thay đổi | Căn chỉnh mục theo đơn vị ghi flash. |

### `schema_config1`

| Bit | Ý nghĩa | Mục đích |
|---:|---|---|
| 7 | Dành riêng; ghi `0`. | Tùy chọn tương lai. |
| 6 | `byte_order`: `0` = little-endian; `1` = big-endian. | Thứ tự byte của giá trị nhiều byte. |
| 5 | Dành riêng; ghi `0`. | Tùy chọn tương lai. |
| 4 | `col_scale_mul`: `1` = có; `0` = không. | Bật hệ số nhân theo cột. |
| 3 | `col_scale_ratios`: `1` = có; `0` = không. | Bật tỷ lệ theo cột. |
| 2 | `col_scale_exp`: `1` = có; `0` = không. | Bật số mũ theo cột. |
| 1 | Dành riêng; ghi `0`. | Tùy chọn tương lai. |
| 0 | `col_signs`: `1` = dùng; `0` = bỏ qua. | Nếu bỏ qua, tất cả cột là không dấu. |

### Byte `codec`

Mỗi header `R` hoặc `C` có một byte `codec` riêng. Các bit đề xuất tách cách biểu diễn giá trị khỏi các bước nén không mất dữ liệu:

| Bit | Ý nghĩa | Mục đích |
|---:|---|---|
| 7–4 | Dành riêng; ghi `0`. | Dành chỗ cho cách mã hóa trong tương lai. |
| 3 | `rle`: `0` = tắt; `1` = bật. | Nén các kết quả lặp lại sau bước dự đoán. |
| 2–1 | `predictor`: `00` = không; `01` = delta; `10` = delta-of-delta; `11` = dành riêng. | Mã hóa biến đổi giữa các giá trị liên tiếp của từng cột. |
| 0 | `representation`: `0` = raw; `1` = lượng tử hóa đều. | Chọn giá trị đưa vào bước dự đoán. |

Ví dụ: `0x00` là raw, `0x01` là lượng tử hóa đều, `0x03` là lượng tử hóa cộng delta, và `0x0B` thêm RLE. Reader từ chối tổ hợp dành riêng hoặc chưa hỗ trợ.

Thứ tự mã hóa: giá trị raw hoặc lượng tử hóa đều → dự đoán → RLE → payload. Giải mã theo thứ tự ngược lại. Delta bắt đầu bằng một giá trị chưa dự đoán cho mỗi cột; delta-of-delta bắt đầu bằng hai giá trị. Trạng thái dự đoán được đặt lại ở đầu mỗi khối, nên từng khối có thể giải mã riêng. RLE áp dụng cho kết quả dự đoán; `chest_count` là số Chest sau giải mã. Delta và RLE giữ nguyên giá trị đầu vào, còn lượng tử hóa đều có thể gây sai số làm tròn.

Lượng tử hóa đều cần lưu khoảng giá trị và số bit cho từng cột trong schema. Cách lưu siêu dữ liệu, đóng gói bit, mã hóa sai phân có dấu, định dạng RLE và xử lý giá trị ngoài khoảng vẫn cần định nghĩa. Cho đến lúc đó, chỉ ghi `codec = 0x00`; reader từ chối giá trị khác. Xem [kế hoạch mã hóa](README-ARCHIVE.md#live-and-archive-encoding-plan).

### Mã đơn vị `col_units`

Mỗi mã đơn vị chiếm 1 byte:

| Mã | Đơn vị, theo thứ tự mã |
|---:|---|
| `30–32` | Hertz (Hz), kilohertz (kHz), gigahertz (GHz) |
| `23–29` | Giây, Phút, Giờ, Ngày, Tháng lịch, Năm lịch, Tuần |
| `20–22` | Nano giây, Micro giây, Mili giây |
| `17–19` | Ounce, Pound, Short ton (US) |
| `13–16` | Inch, Foot, Yard, Mile |
| `10–12` | Kelvin (K), Celsius (°C), Fahrenheit (°F) |
| `5–9` | Lux, dBm, dB, Độ (°), Radian |
| `3–4` | Phần trăm (%), Phần triệu (ppm) |
| `2` | Thaums |
| `0–1` | Không xác định, Số đếm |

Schema trả lời câu hỏi: với một `schema_id`, byte của payload được diễn giải thế nào?

```text
payload     = 0x01 0x02 0x03 0x04
temperature = 0x01 0x02
humidity    = 0x03 0x04
```

Ví dụ schema ba cột:

```text
schema_id      = 0
col_count      = 3
schema_config1 = 0b00000001  # bật col_signs
col_widths     = [1, 2, 4]    # byte mỗi cột
col_signs      = 0b01000000
col_units      = [0, 0, 0]    # đơn vị chưa xác định
```

Cột thứ nhất là số nguyên 8 bit không dấu, cột thứ hai là số nguyên 16 bit có dấu, và cột thứ ba là số nguyên 32 bit không dấu. Khi đổi schema, ghi mục mới vào cả hai Superblock. Dữ liệu dùng schema mới bắt đầu ở một khối `R` mới.

## Khối Record (`R`)

Mỗi `dataset_id` chỉ gắn với một `interval_code` trong suốt vòng đời của log. Các khoảng thời gian khác nhau của cùng một loại dữ liệu dùng các `dataset_id` khác nhau. Khối `R` dùng một `schema_id` đã xác nhận ở cả hai Superblock và thuộc luồng xác định bởi `dataset_id`. Các luồng dùng chung vùng khối.

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte* | Đánh dấu đầu khối. |
| **crc16** | 2 byte** | Bao phủ header và payload, gồm `chest_count`; không gồm chính nó. |
| **codec** | 1 byte* | Chọn cách biểu diễn giá trị và các bước nén của khối; xem [byte `codec`](#byte-codec). |
| **schema_id** | 1 byte* | Chọn schema của khối. |
| **dataset_id** | 1 byte* | ID bộ dữ liệu. |
| **interval_code** | 1 byte* | Chọn khoảng thời gian của luồng. |
| **stream_counter** | 4 byte* | Thứ tự khối trong luồng. |
| **chest_count** | 2 byte* | Số Chest trong khối; ghi khi hoàn tất để xác định ranh giới các vùng payload. |
| **record_config** | 5 byte** | Thời gian và định dạng thời gian của khối. |
| **payload** | Thay đổi | Chứa Chest theo codec đã chọn; xem [Payload](#payload). |

Header khối `R` chiếm 19 byte. Khi hoàn tất khối, ghi `chest_count`, sau đó tính và lưu `crc16`; reader chỉ dùng số đếm nếu CRC hợp lệ.

## Cấu hình Record

`record_config` chiếm 5 byte:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| `time_value` | 4 byte | Giá trị thời gian của khối. |
| `time_format` | 1 byte | Cách biểu diễn thời gian. |

## Khối Continuation (`C`)

Khối `C` chứa các bản ghi nối tiếp của một extent khi payload khối `R` đầy. `dataset_id` và `stream_counter` giúp reader nối và sắp xếp các khối dù nhiều luồng xen kẽ. Khối `C` lấy `interval_code` và `schema_id` từ khối `R` trước đó trong cùng luồng. Xem [quy tắc khôi phục luồng](README.md#recovery-and-log-scanning).

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte* | Đánh dấu đầu khối. |
| **crc16** | 2 byte** | Bao phủ header và payload, gồm `chest_count`; không gồm chính nó. |
| **dataset_id** | 1 byte* | ID bộ dữ liệu. |
| **stream_counter** | 4 byte* | Thứ tự khối trong luồng. |
| **chest_count** | 2 byte* | Số Chest trong khối; ghi khi hoàn tất để xác định ranh giới các vùng payload. |
| **codec** | 1 byte* | Chọn cách biểu diễn giá trị và các bước nén của khối; xem [byte `codec`](#byte-codec). |
| **payload** | Thay đổi | Chứa Chest theo schema của khối `R` cùng extent và codec của khối này. |

Header khối `C` chiếm 12 byte. Các ước tính sau áp dụng cho codec `0`:

| Kích thước Chest | Bản ghi/khối `C` | Byte payload chưa dùng |
|---:|---:|---:|
| 4 byte | 1.020 | 2 |
| 8 byte | 510 | 2 |
| 12 byte | 340 | 2 |
| 16 byte | 255 | 2 |
| 20 byte | 204 | 2 |
| 24 byte | 170 | 2 |

## Payload

Với codec `0`, Chest được ghi liên tiếp từ đầu payload của mỗi khối `R` hoặc `C`:

```text
[header khối] [Chest 0] [Chest 1] [Chest 2] ...
```

`col_widths` xác định kích thước các cột trong Chest raw. Khi hoàn tất khối, ghi `chest_count` 2 byte vào header, sau đó tính và lưu `crc16` trên header và payload, không bao gồm chính trường CRC. Reader dùng số đếm đã xác thực cùng schema để tìm cuối vùng Chest; phần còn lại của payload chưa dùng. Codec nén trong tương lai phải định nghĩa bố cục byte và cách số đếm tương ứng với các Chest sau giải mã.

## Ước tính sức chứa

Mỗi Superblock có 4.084 byte cho `schema_entries`. Ước tính dưới đây giả định mỗi mục được căn chỉnh 4 byte, có `col_signs` và `col_scale_ratios`; các mảng tùy chọn khác không có. 6 byte cố định là tổng kích thước các trường áp dụng cho toàn schema. Mỗi cột thêm 4 byte cho `col_widths`, `col_units` và `col_scale_ratios`.

| `col_count` | Kích thước mục đã căn chỉnh | Mục mỗi Superblock | Byte còn trống mỗi Superblock |
|---:|---:|---:|---:|
| 4 | 24 byte | 170 | 4 |
| 8 | 40 byte | 102 | 4 |
| 16 | 72 byte | 56 | 52 |
| 32 | 140 byte | 29 | 24 |
| 64 | 272 byte | 15 | 4 |

Số mục bao gồm schema ban đầu, nên số lần cập nhật schema tối đa là `số mục − 1`. Hai Superblock lưu cùng các mục; chúng là bản sao, không phải dung lượng bổ sung.

Với 12 cột có độ rộng bằng nhau, payload khối `C` chứa:

| Độ rộng/cột | Kích thước Chest | Chest/khối `C` | Byte chưa dùng |
|---:|---:|---:|---:|
| 1 byte | 12 byte | 340 | 2 |
| 2 byte | 24 byte | 170 | 2 |
| 4 byte | 48 byte | 85 | 2 |
| 8 byte | 96 byte | 42 | 50 |

Với 6 cột có độ rộng bằng nhau:

| Độ rộng/cột | Kích thước Chest | Chest/khối `C` | Byte chưa dùng |
|---:|---:|---:|---:|
| 1 byte | 6 byte | 680 | 2 |
| 2 byte | 12 byte | 340 | 2 |
| 4 byte | 24 byte | 170 | 2 |
| 8 byte | 48 byte | 85 | 2 |

Bảng giả định mỗi Chest raw có một cột 4 byte, được lưu cách nhau 1, 10 hoặc 30 giây. Số Chest bao gồm sức chứa của 1 khối Record và tất cả khối Continuation. Đơn vị dung lượng là nhị phân.

| Dung lượng | Khối `C` | Dung lượng header `C` | Bản ghi | Thời gian (1 giây) | Thời gian (10 giây) | Thời gian (30 giây) |
|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 0,06 KiB | 6.118 | ~1,7 giờ | ~17,0 giờ | ~2,1 ngày |
| 128 KiB | 29 | 0,3 KiB | 30.598 | ~8,5 giờ | ~3,5 ngày | ~10,6 ngày |
| 1 MiB | 253 | 3,0 KiB | 259.078 | ~3,0 ngày | ~30,0 ngày | ~90,0 ngày |
| 32 MiB | 8.189 | 96 KiB | 8,4 × 10⁶ | ~96,7 ngày | ~2,6 năm | ~7,9 năm |
| 126 MiB | 32.253 | 378 KiB | 32,9 × 10⁶ | ~1,0 năm | ~10,4 năm | ~31,3 năm |
| 512 MiB | 131.069 | 1.536 KiB | 133,7 × 10⁶ | ~4,2 năm | ~42,4 năm | ~127,1 năm |
| 1 GiB | 262.141 | 3.072 KiB | 267,4 × 10⁶ | ~8,5 năm | ~84,7 năm | ~254,2 năm |

> Giả sử có 2 Superblock và 1 khối `R`; các khối 4 KiB còn lại đều là `C`. Với codec `0`, khối `R` chứa 1.018 Chest và mỗi khối `C` chứa 1.020 Chest. Nhiều luồng cần thêm khối `R`.

