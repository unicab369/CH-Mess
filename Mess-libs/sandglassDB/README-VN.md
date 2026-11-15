# SandglassDB

## Tổng quan

Cơ sở dữ liệu bắt đầu bằng 2 Superblock (`S0`, `S1`). Các khối còn lại thuộc một vùng lưu trữ chung. Khối Record (`R`) bắt đầu một extent; các khối Continuation (`C`) nối dài extent đó khi payload đầy. `stream_id` xác định luồng độ phân giải, còn `stream_sequence` xác định thứ tự khối trong từng luồng. Khối của nhiều luồng có thể xen kẽ nhau.

**Chest** là tập giá trị cột của một bản ghi, nằm trong vùng dữ liệu đọc của payload `R` hoặc `C`. **Rune** là hệ số 1 byte gắn với một vị trí bit. **Essence** là mặt nạ bit được đóng gói trong payload cho từng cột và lần đọc. Mỗi bit Essence đóng góp Rune tương ứng với dấu: `1` nghĩa là `+1 × Rune`; `0` nghĩa là `−1 × Rune`. Với Rune `[a, b, c, d]`, Essence `1010` cho kết quả `a − b + c − d`.

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
| **codec** | 1 byte* | Chọn cách mã hóa payload cho các khối dùng schema này; dùng 3 bit thấp, 5 bit cao bằng `0`. |

Trường theo cột, theo thứ tự trong mục:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **col_widths** | 1 byte / cột | Số byte cấp cho mỗi cột trong Chest. |
| **col_units** | 1 byte* / cột | Mã đơn vị của mỗi cột; xem [mã đơn vị](#mã-đơn-vị-col_units). |
| **col_scale_mul** | 1 byte** / cột | Có khi `schema_config1` bit 4 bằng `1`; hệ số nhân mặc định là `1`. Ví dụ `10 × 3 = 30`. |
| **col_scale_exp** | 1 byte có dấu** / cột | Có khi bit 2 bằng `1`; số mũ cơ số 10 mặc định là `0`. Ví dụ `123 × 10⁻² = 1,23`. |
| **col_scale_ratios** | 2 byte** / cột | Có khi bit 3 bằng `1`; cặp tử số/mẫu số mặc định là `1/1`. Ví dụ `10 × 3/2 = 15`. |
| **col_signs** | `ceil(col_count / 8)` byte* | Một bit cho mỗi cột (`1` = có dấu, `0` = không dấu); dùng khi `schema_config1` bit 0 bằng `1`. Bit thừa bằng `0`. |
| **col_essence_bits** | 1 byte** / cột | Có khi `schema_config1` bit 1 bằng `1`; số bit của Essence (`0` = không có; `1`, `2` hoặc `4` bit mỗi lần đọc). |
| **col_runes** | 1 byte × tổng số bit Essence** | Có khi `schema_config1` bit 1 bằng `1`; các hệ số Rune theo thứ tự cột rồi đến vị trí bit. |
| **padding** | Thay đổi | Căn chỉnh mục theo đơn vị ghi flash. |

Tổng số bit Essence là tổng `col_essence_bits` của các cột. Ví dụ 3 cột, mỗi cột có Essence 2 bit, cần 6 Rune chiếm 6 byte. Rune được lưu một lần trong mục Schema; mỗi bit Essence xác định dấu của Rune tương ứng (`1` cộng, `0` trừ).

### `schema_config1`

| Bit | Ý nghĩa | Mục đích |
|---:|---|---|
| 7 | Dành riêng; ghi `0`. | Tùy chọn tương lai. |
| 6 | `byte_order`: `0` = little-endian; `1` = big-endian. | Thứ tự byte của giá trị nhiều byte. |
| 5 | Dành riêng; ghi `0`. | Tùy chọn tương lai. |
| 4 | `col_scale_mul`: `1` = có; `0` = không. | Bật hệ số nhân theo cột. |
| 3 | `col_scale_ratios`: `1` = có; `0` = không. | Bật tỷ lệ theo cột. |
| 2 | `col_scale_exp`: `1` = có; `0` = không. | Bật số mũ theo cột. |
| 1 | `col_essence_bits`: `1` = có; `0` = không. | Bật `col_essence_bits` và `col_runes`. |
| 0 | `col_signs`: `1` = dùng; `0` = bỏ qua. | Nếu bỏ qua, tất cả cột là không dấu. |

### ID codec

| ID | Codec | Ý nghĩa |
|---:|---|---|
| `7`–`3` | Dành riêng | Không ghi; reader xem các ID này là chưa hỗ trợ. |
| `2` | Delta + varint | Lưu bản ghi đầu ở dạng raw, sau đó lưu sai phân có dấu dưới dạng số nguyên độ dài thay đổi. |
| `1` | Run-length encoding (RLE) | Lưu một bản ghi lặp lại kèm số lần lặp. |
| `0` | Raw | Lưu theo schema, không biến đổi; mặc định. |

Codec áp dụng cho mọi khối `R` và `C` dùng schema này. Trước khi định nghĩa và triển khai cách giải mã phi raw, ghi ID `0`.

### Mã đơn vị `col_units`

Mỗi mã đơn vị chiếm 1 byte:

| Mã | Đơn vị, theo thứ tự mã |
|---:|---|
| `0–1` | Không xác định, Số đếm |
| `2` | Thaums |
| `3–4` | Phần trăm (%), Phần triệu (ppm) |
| `5–9` | Lux, dBm, dB, Độ (°), Radian |
| `10–12` | Kelvin (K), Celsius (°C), Fahrenheit (°F) |
| `13–16` | Inch, Foot, Yard, Mile |
| `17–19` | Ounce, Pound, Short ton (US) |
| `20–22` | Nano giây, Micro giây, Mili giây |
| `23–29` | Giây, Phút, Giờ, Ngày, Tháng lịch, Năm lịch, Tuần |
| `30–32` | Hertz (Hz), kilohertz (kHz), gigahertz (GHz) |

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
codec          = 0            # Raw
```

Cột thứ nhất là số nguyên 8 bit không dấu, cột thứ hai là số nguyên 16 bit có dấu, và cột thứ ba là số nguyên 32 bit không dấu. Khi đổi schema, ghi mục mới vào cả hai Superblock. Dữ liệu dùng schema mới bắt đầu ở một khối `R` mới.

## Khối Record (`R`)

Khối `R` dùng một `schema_id` đã xác nhận ở cả hai Superblock và thuộc luồng `stream_id`. Các luồng dùng chung vùng khối.

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte* | Đánh dấu đầu khối. |
| **crc16** | 2 byte** | Bao phủ header và payload, gồm `chest_count`; không gồm chính nó. |
| **schema_id** | 1 byte* | Chọn schema của khối. |
| **stream_id** | 1 byte* | ID luồng độ phân giải. |
| **stream_sequence** | 4 byte* | Thứ tự khối trong luồng. |
| **chest_count** | 2 byte* | Số Chest trong khối; ghi khi hoàn tất để xác định ranh giới các vùng payload. |
| **record_config** | 5 byte** | Thời gian và định dạng thời gian của khối. |
| **payload** | Thay đổi | Chứa Chest và Essence được đóng gói khi bật; xem [Payload](#payload). |

Header khối `R` chiếm 17 byte. Khi hoàn tất khối, ghi `chest_count`, sau đó tính và lưu `crc16`; reader chỉ dùng số đếm nếu CRC hợp lệ.

`col_essence_bits` nằm trong schema; Essence nằm trong payload của khối.

## Cấu hình Record

`record_config` chiếm 5 byte:

| Trường | Kích thước | Mục đích |
|---|---:|---|
| `time_value` | 4 byte | Giá trị thời gian của khối. |
| `time_format` | 1 byte | Cách biểu diễn thời gian. |

## Khối Continuation (`C`)

Khối `C` chứa các bản ghi nối tiếp của một extent khi payload khối `R` đầy. `stream_id` và `stream_sequence` giúp reader nối và sắp xếp các khối dù nhiều luồng xen kẽ. Xem [quy tắc khôi phục luồng](README.md#recovery).

| Trường | Kích thước | Mục đích |
|---|---:|---|
| **magic_number** | 2 byte* | Đánh dấu đầu khối. |
| **crc16** | 2 byte** | Bao phủ header và payload, gồm `chest_count`; không gồm chính nó. |
| **stream_id** | 1 byte* | ID luồng độ phân giải. |
| **stream_sequence** | 4 byte* | Thứ tự khối trong luồng. |
| **chest_count** | 2 byte* | Số Chest trong khối; ghi khi hoàn tất để xác định ranh giới các vùng payload. |
| **payload** | Thay đổi | Chứa Chest và Essence theo schema của khối `R` cùng extent. |

Header khối `C` chiếm 11 byte. Các ước tính sau không tính vùng Essence:

| Kích thước Chest | Bản ghi/khối `C` | Byte payload chưa dùng |
|---:|---:|---:|
| 4 byte | 1.020 | 3 |
| 8 byte | 510 | 3 |
| 12 byte | 340 | 3 |
| 16 byte | 255 | 3 |
| 20 byte | 204 | 3 |
| 24 byte | 170 | 3 |

## Payload

Mỗi khối `R` hoặc `C` có hai vùng payload. Chest được ghi từ đầu vùng về phía trước; Essence được đóng gói từ cuối vùng về phía sau:

```text
[header khối] [Chest tăng →             ][← Essence đóng gói]
```

`col_widths` xác định kích thước các cột trong Chest. Khi `schema_config1` bit 1 bật `col_essence_bits`, mỗi giá trị khác `0` xác định số bit Essence cho cột đó trong mỗi lần đọc. Essence được đóng gói theo thứ tự Chest. Mỗi khối `C` có vùng Essence riêng.

Khi thêm Chest, ghi Essence từ cuối payload về phía đầu. Khi hoàn tất khối, ghi `chest_count` 2 byte vào header, sau đó tính và lưu `crc16` trên header và payload, không bao gồm chính trường CRC. Reader dùng `chest_count` cùng schema để xác định hai vùng; khoảng trống giữa chúng là payload chưa dùng.

Xem [README-QUANTIZATION.md](README-QUANTIZATION.md) để biết sức chứa khi dùng Essence 2 bit hoặc 4 bit.

## Ước tính sức chứa

Mỗi Superblock có 4.084 byte cho `schema_entries`. Ước tính dưới đây giả định mỗi mục được căn chỉnh 4 byte, có `col_signs`, `col_scale_ratios`, Essence 4 bit và Rune 1 byte; các mảng tùy chọn khác không có. 7 byte cố định là tổng kích thước các trường áp dụng cho toàn schema. Mỗi cột thêm 2 byte: 1 byte `col_widths` và 1 byte `col_units`.

| `col_count` | Kích thước mục đã căn chỉnh | Mục mỗi Superblock | Byte còn trống mỗi Superblock |
|---:|---:|---:|---:|
| 4 | 44 byte | 92 | 36 |
| 8 | 80 byte | 51 | 4 |
| 16 | 156 byte | 26 | 28 |
| 32 | 300 byte | 13 | 184 |
| 64 | 592 byte | 6 | 532 |

Số mục bao gồm schema ban đầu, nên số lần cập nhật schema tối đa là `số mục − 1`. Hai Superblock lưu cùng các mục; chúng là bản sao, không phải dung lượng bổ sung.

Với 12 cột có độ rộng bằng nhau, payload khối `C` chứa:

| Độ rộng/cột | Kích thước Chest | Chest/khối `C` | Byte chưa dùng |
|---:|---:|---:|---:|
| 1 byte | 12 byte | 340 | 3 |
| 2 byte | 24 byte | 170 | 3 |
| 4 byte | 48 byte | 85 | 3 |
| 8 byte | 96 byte | 42 | 51 |

Với 6 cột có độ rộng bằng nhau:

| Độ rộng/cột | Kích thước Chest | Chest/khối `C` | Byte chưa dùng |
|---:|---:|---:|---:|
| 1 byte | 6 byte | 680 | 3 |
| 2 byte | 12 byte | 340 | 3 |
| 4 byte | 24 byte | 170 | 3 |
| 8 byte | 48 byte | 85 | 3 |

Bảng giả định mỗi bản ghi có một cột 4 byte và một Essence 4 bit, được lưu cách nhau 1, 10 hoặc 30 giây. Số bản ghi bao gồm sức chứa của khối Record và tất cả khối Continuation. Đơn vị dung lượng là nhị phân.

| Dung lượng | Khối `C` | Dung lượng header `C` | Bản ghi | Thời gian (1 giây) | Thời gian (10 giây) | Thời gian (30 giây) |
|---:|---:|---:|---:|---:|---:|---:|
| 32 KiB | 5 | 0,05 KiB | 5.441 | ~1,5 giờ | ~15,1 giờ | ~1,9 ngày |
| 128 KiB | 29 | 0,3 KiB | 27.209 | ~7,6 giờ | ~3,1 ngày | ~9,4 ngày |
| 1 MiB | 253 | 2,7 KiB | 230.377 | ~2,7 ngày | ~26,7 ngày | ~80,0 ngày |
| 32 MiB | 8.189 | 88 KiB | 7,4 × 10⁶ | ~86,0 ngày | ~2,4 năm | ~7,1 năm |
| 126 MiB | 32.253 | 346 KiB | 29,3 × 10⁶ | ~338,6 ngày | ~9,3 năm | ~27,8 năm |
| 512 MiB | 131.069 | 1.408 KiB | 118,9 × 10⁶ | ~3,8 năm | ~37,7 năm | ~113,1 năm |
| 1 GiB | 262.141 | 2.816 KiB | 237,8 × 10⁶ | ~7,5 năm | ~75,4 năm | ~226,2 năm |

> Giả sử có 2 Superblock và 1 khối `R`; các khối 4 KiB còn lại đều là `C`. Mỗi khối `R` chứa 906 bản ghi và mỗi khối `C` chứa 907 bản ghi, đã tính vùng Essence 4 bit và `chest_count` 2 byte. Đây là giới hạn trên vì nhiều luồng cần thêm khối `R`.

