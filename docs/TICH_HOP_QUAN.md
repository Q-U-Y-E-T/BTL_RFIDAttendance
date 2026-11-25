# Tích hợp bộ lọc UID của Quân

Nguồn đối chiếu: nhánh feature/rfid-task-queue tại bfbf3c3c2107754314ebf6b89e3dab5bdfe6df27, chứa commit bc2f08c33f5f6d935357e757ea20bbcd72b60dc6 (Add 2-minute cooldown per RFID UID). Chỉ ghép logic cần thiết vào bản tích hợp; không thay Makefile hay ứng dụng bằng bản test riêng của Quân.

## Hành vi

- Nhận lần đầu, rồi cùng UID phải chờ ít nhất 120000 ms từ lúc đưa sự kiện vào queue thành công. Các lần đọc bị chặn không kéo dài mốc chờ.
- Giữ thẻ liên tục vẫn được nhận lại sau 2 phút; rút rồi đưa lại trước hạn không vượt qua bộ lọc. Mỗi UID có mốc riêng; không chặn đọc thẻ khác trong 2 phút.
- Tối đa 50 UID đang chờ, gồm cả thẻ lạ/thẻ gặp lỗi SD. Đầy bảng thì chặn UID mới và tăng cooldown_full, không loại bỏ UID chưa hết hạn. Slot hết hạn được dùng lại.
- Bộ lọc đặt trước xử lý SD: cả lượt thẻ lạ hoặc lượt không ghi được vì lỗi SD cũng bắt đầu thời gian chờ. Không tự ghi bù lượt bị lỗi; quét lại sau thời gian chờ. RESET xóa bảng chờ trong RAM, không xóa history.csv.
- Cấu hình chung ở App/Inc/card_cooldown.h. Không có delay 2 phút; task vẫn thăm dò mỗi 50 ms.

## Phần được giữ

- Ba task FreeRTOS, main chỉ khởi tạo và gọi chạy; RFID SPI1, Storage SPI2, LCD I2C1.
- Tự nhúng Tools/Provision/DanhSach.csv, đồng bộ khi khởi động/cắm lại SD, lưu backup, kiểm tra CSV và giữ history.csv.
- Tra tên theo UID, chỉ báo thành công sau khi ghi/đóng file thành công; LCD, PC13 và PA8 phản hồi như trước.
- Thử lại LCD, tự khởi tạo lại RC522 sau nhiều lỗi, xử lý mất/cắm lại SD và queue giữ sự kiện khi đầy.

## Điểm ghép

Bảng chờ chỉ có một bản do task RFID sở hữu, thêm khoảng 800 byte RAM trên ARM 32 bit. ReaderEvent_t thêm cờ suppressed; sự kiện hiện diện bị chặn vẫn đi qua queue để hủy chuỗi vắng thẻ nhưng không ghi log hoặc phát kết quả LCD/LED. Thống kê cards_suppressed/cooldown_full có trong g_rfid_task_stats.

Attendance cũ khóa vô hạn thẻ đang giữ; bản này mở khóa khi đủ 120 giây. App_SubmitCardAt/Attendance_ProcessCardAt dùng observed_ms tại phía gửi, không lấy thời gian xử lý của task Storage làm mốc. Nhờ vậy SD/queue chậm không khiến lượt đúng hạn bị chặn thêm 2 phút. API cũ vẫn có wrapper để tương thích; ứng dụng RTOS đi qua AppInput_Process và bộ lọc RFID, không gọi thẳng wrapper để bỏ qua bảng theo UID.

## Kiểm thử

make check: 26 nhóm core + 21 nhóm định tuyến task/UI/cooldown + 14 nhóm cập nhật file + 8 nhóm đồng bộ danh sách = 69 nhóm C. Thêm kiểm tra generator, luồng Makefile và cú pháp 42 đơn vị C. Kiểm thử C dùng AddressSanitizer/UndefinedBehaviorSanitizer, FatFs thật trên đĩa RAM và HAL/queue giả lập có điều khiển.

Bổ sung 8 nhóm: giữ thẻ ở 119999/120000 ms, đếm dòng log và kết quả, rút/cắm trước hạn, hai UID độc lập, queue chậm và mốc enqueue, bảng đầy 50 UID, UID 4/7/10 byte và tràn tick, timeout tại tầng Attendance khi tick tràn.

Chưa build/link ARM hoặc đo phần cứng trong môi trường sửa này. Trên Ubuntu cần kiểm tra make và số RAM thực sau liên kết; không dùng số RAM của bản cũ làm số đo cho bản này. Bộ lọc dùng tick 32 bit, kiểm thử qua một lần tràn; không lưu trạng thái qua reset.

## Chạy trên máy Quyết

Bản gốc được giữ riêng, không ghi đè. Trong bản mới, Tools/Provision/DanhSach.csv vẫn giữ nguyên dữ liệu đầu vào; hiện chỉ có tiêu đề, phải điền ít nhất một sinh viên với UID, mã và tên thật. Không tự gán tên khi chưa có thông tin.

```sh
make check
make -j4 && st-flash write build-rtos/RFIDAttendance.bin 0x08000000
```

Dùng && để chỉ nạp sau khi build thành công. Không cần firmware nạp danh sách riêng. Khi thử trên board: thẻ A lần đầu báo, giữ hoặc rút/cắm trước 2 phút không báo lại; thẻ B vẫn báo; sau 2 phút A báo lại. Kiểm tra history.csv có đúng số lượt và thử mất/cắm lại SD.
