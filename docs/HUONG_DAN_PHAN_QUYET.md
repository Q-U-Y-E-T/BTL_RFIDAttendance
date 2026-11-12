# Hướng dẫn phần riêng của Quyết

## 1. Những gì có trong bản này

- SD qua SPI2 và FatFs: đọc danh sách khi khởi động, phát hiện mất kết nối và đọc lại khi cắm thẻ.
- Tra UID trong danh sách; chỉ báo thành công sau khi ghi và đồng bộ/đóng history.csv thành công.
- UID lặp liên tiếp bị chặn; lỗi ghi cũng không tự thử ghi lại cùng lượt quét.
- Phản hồi một nhịp khi thành công, ba nhịp khi thẻ lạ hoặc lỗi; mỗi nhịp sáng 250 ms, cách nhau 250 ms.
- Kiểm thử tự động dùng code ứng dụng và FatFs thật trên đĩa RAM, có giả lập lỗi đọc/ghi và cắm/rút.

Đây là bản vòng lặp đơn. Driver RFID của Quân, LCD của Sơn và phần FreeRTOS/queue chưa được ghép. Không gọi đồng thời các hàm Storage/App từ nhiều task.

## 2. Đưa code vào máy

Giải nén gói thành một thư mục riêng trước, không xóa repository hiện tại. Trong repository thật, chạy `git status` và lưu các thay đổi đang làm trước khi chép file.

Bản này thay đổi các file trong App/, Modules/Storage/storage.c, Tests/, docs/, README.md, Makefile và .gitignore. Có thể so sánh với repository trước khi chép. Giữ thư mục .git của repository thật; không tạo repository Git mới. Không ghi đè driver RC522/LCD mà bạn khác đang phát triển.

Các thư viện HAL/FatFs, cấu hình chân và main.c được giữ theo bản gửi lên. main.c gọi App_Init() sau khởi tạo GPIO/SPI/I2C, rồi gọi App_Process() liên tục và HAL_Delay(5).

## 3. Build và nạp thử

Trên Ubuntu có GCC, Python 3 và bộ công cụ ARM đã dùng trước đây:

```sh
make test-host
make clean-all
make -j4
st-flash write build/RFIDAttendance.bin 0x08000000
```

Bản mặc định LED tắt khi không có sự kiện; đây không phải dấu hiệu chương trình treo. Quản lý SD vẫn chạy.

Muốn thử riêng khi chưa ghép RFID:

```sh
make DEMO=1 -j4
st-flash write build-demo/RFIDAttendance.bin 0x08000000
```

Nếu SD có danh sách hợp lệ: sau khoảng 2 giây dùng UID sinh viên đầu tiên, ghi một dòng và báo một nhịp; thử lại cùng UID không ghi thêm; sau khoảng 3 giây tính từ lần đầu thử UID lạ và báo ba nhịp. Sau đó kết thúc demo cho đến reset. Chế độ này tạo dữ liệu giả; dùng thẻ/test log riêng. Không tự tạo hoặc ghi đè DanhSach.csv.

Quay lại bản thường bằng `make -j4` rồi nạp file trong build/, không nạp nhầm build-demo/.

## 4. Danh sách và log

DanhSach.csv nằm ở gốc thẻ, tiêu đề chính xác:

```csv
uid,student_id,student_name
A1B2C3D4,SV001,NGUYEN VAN A
11223344,SV002,TRAN THI B
```

UID trên là giả. Thay bằng UID thật do Quân đọc và gán đúng sinh viên. Không cần biết tất cả UID mới viết được phần Storage.

Giới hạn: 50 sinh viên; UID 4/7/10 byte viết bằng 8/14/20 ký tự HEX HOA, giữ số 0 đầu; ID tối đa 15 ký tự; tên tối đa 48 ký tự ASCII không dấu. Không dấu phẩy, dấu ngoặc kép, dòng trống hoặc BOM. Tên toàn dấu cách không hợp lệ. Chấp nhận LF/CRLF và dòng cuối không có newline. UID trùng, danh sách quá lớn hoặc sai định dạng bị từ chối toàn bộ; không dùng danh sách đọc dở.

history.csv có tiêu đề `uptime_ms,uid,student_id`, chỉ ghi thêm. uptime_ms là thời gian từ lúc khởi động, không phải ngày giờ thực. Nếu file log sai tiêu đề hoặc dòng cuối dang dở, báo lỗi và không nối thêm; sao lưu/kiểm tra file trên máy trước khi sửa. Không tự xóa lịch sử.

Đọc lại danh sách khi khởi động/cắm thẻ; không theo dõi việc sửa file trên thẻ liên tục khi thẻ vẫn được coi là đang kết nối. Không chỉnh file bằng thiết bị khác khi STM32 đang truy cập thẻ.

## 5. Giao diện ghép với Quân và Sơn

Tất cả chạy trong cùng vòng lặp/task sở hữu App:

1. Khởi tạo ngoại vi, App_Init(), rồi driver RFID/LCD.
2. Gọi App_Process() thường xuyên để chạy phản hồi và quản lý SD.
3. Khi đọc được UID hợp lệ, tạo CardEvent_t và gọi App_SubmitCard(&card).
4. Gọi App_TakeDisplayEvent(&display) để lấy kết quả cho LCD. Chỉ hiển thị khi hàm trả 1. Kết quả được sao chép, không giữ con trỏ vào danh sách nội bộ.
5. Khi đầu đọc xác nhận không có thẻ, gọi App_NotifyNoCard(HAL_GetTick()) mỗi lần thăm dò, khoảng 100 ms hoặc nhanh hơn. Khi lỗi đầu đọc, gọi App_NotifyReaderError().

ATTENDANCE_BUSY nghĩa là đang phát phản hồi hoặc còn kết quả LCD chưa lấy; bên gọi cần giữ sự kiện và thử lại sau, không coi là đã điểm danh. ATTENDANCE_DUPLICATE nghĩa là đã chặn lượt lặp, không có log/phản hồi/LCD mới. Phải lấy kết quả LCD kể cả khi tạm chưa có LCD thật, để App nhận lượt tiếp theo. Bản hiện tại chỉ giữ một kết quả, chưa có queue.

Cùng UID chỉ được nhận lại sau chuỗi báo vắng liên tục ít nhất 600 ms, các lần báo cách nhau không quá 250 ms. UID khác có thể được nhận ngay khi App hết bận. Đây không phải quy tắc mỗi sinh viên chỉ được điểm danh một lần mỗi buổi.

CẦN THỐNG NHẤT VỚI QUÂN: RFID_NO_CARD hiện chỉ được mô tả là không đọc được thẻ trong một lần thăm dò. Không được mặc định coi lỗi đọc hoặc thẻ đã HALT là đã rút thẻ. Driver phải kiểm tra hiện diện phù hợp (kể cả thẻ đã HALT) trước khi gửi chuỗi báo vắng. Nếu chưa xác nhận được điều này, chống lặp trên phần cứng chưa được bảo đảm chỉ bởi test ứng dụng.

Khi ghi lỗi: báo lỗi, vô hiệu trạng thái SD, phục hồi kết nối; không tự ghi lại cùng UID đang giữ. Người dùng đưa thẻ ra rồi quét lại sau khi SD sẵn sàng. Không thể bảo đảm ghi đúng một lần nếu mất điện giữa thao tác; cần kiểm tra file sau sự cố.

## 6. Phần cứng đang dùng

| Tín hiệu | Chân STM32 |
|---|---|
| SD CS | PB12 |
| SD SCK | PB13 |
| SD MISO | PB14 |
| SD MOSI | PB15 |
| LED trên board | PC13, mức thấp sáng |
| LED ngoài mô phỏng còi | PA8, mức cao sáng |

LED PA8 đi qua điện trở hạn dòng rồi xuống GND. Giữ cách cấp nguồn module SD đã thử thành công; module có ổn áp 1117 và nguồn STM32/module chung GND. Khi có còi thật, chọn mạch điều khiển theo điện áp/dòng của còi; PA8 hiện chỉ kiểm thử bằng LED.

Bộ quản lý kiểm tra SD khoảng mỗi giây. Không có chân card-detect: rút/cắm quá nhanh giữa hai lần kiểm tra có thể không được phát hiện. Các lệnh SD là đồng bộ, có thời hạn chờ; lúc kết nối lỗi có thể làm vòng lặp chậm. Tránh rút khi đang ghi. Khi chuyển RTOS cần một task sở hữu SD, truyền sự kiện bằng bản sao và xử lý queue đầy; không gọi driver từ ISR.

## 7. Kiểm tra trước khi push

```sh
git status
make test-host
make clean-all
make -j4
```

Nạp và kiểm tra demo trên board, rút/cắm thẻ, file CSV sai, log thực tế và phản hồi PA8. Sau đó quay về bản mặc định trước khi bàn giao. Commit vào feature/sd-storage, push nhánh đó và mở PR vào develop. Không đưa build/, build-demo/ hoặc build-host/ lên Git. Chưa push hay merge tự động trong bản sửa này.
