# Hướng dẫn bản gọn

## Build

1. Giải nén vào thư mục riêng, mở terminal tại nơi có Makefile.
2. Chạy `make check`. Cần GCC và Python 3 trên laptop. Kết quả phải có đủ 69 nhóm C đạt, kèm kiểm tra 42 file C firmware, ngắt, dữ liệu và dependency build.
3. Chạy `make clean-all` rồi `make -j4`. Cần arm-none-eabi-gcc. Gửi số text/data/bss nếu cần kiểm tra bộ nhớ.
4. Khi build đạt và đã đấu nối đúng, nạp `st-flash write build-rtos/RFIDAttendance.bin 0x08000000`, sau đó nhấn RESET.

Firmware điểm danh mặc định vẫn chỉ có một chế độ. Danh sách được nhúng và tự cập nhật trong cùng firmware, xem NAP_DANH_SACH.md. Không dùng DEMO, RFID_RTOS_TEST hoặc RTOS=0; Makefile báo lỗi rõ nếu nhận các tùy chọn cũ. Object nằm trong thư mục con theo đường dẫn nguồn để tránh trùng tên. Có .d để cập nhật đúng khi header thay đổi; .map/.elf được giữ vì cần kiểm tra RAM/Flash và debug. Không tạo file listing .lst.

Không chép đè nguyên thư mục repository hiện tại khi chưa kiểm tra bản mới. Thao tác chép kiểu cp chỉ thêm/ghi đè, không xóa các file thừa cũ. Sau khi bản riêng build và thử đạt, gửi git status để đồng bộ việc xóa file trên feature/integration. Bản gọn không có .git và không tự commit/push/merge.

## Phần cứng

| Thiết bị | Chân STM32 |
|---|---|
| RC522 CS / RST | PA4 / PB0 |
| RC522 SCK / MISO / MOSI | PA5 / PA6 / PA7 |
| SD CS | PB12 |
| SD SCK / MISO / MOSI | PB13 / PB14 / PB15 |
| LCD SCL / SDA | PB6 / PB7 |
| LED board | PC13, mức thấp sáng |
| LED mô phỏng còi | PA8, mức cao sáng |

Giữ cách cấp nguồn đã thử trước đây, chung GND. RC522 dùng nguồn 3.3 V. LED PA8 phải qua điện trở hạn dòng. Chưa kiểm chứng còi thật; muốn thay LED bằng còi cần kiểm tra dòng/điện áp và mạch điều khiển. Driver LCD thử địa chỉ 7-bit 0x27 rồi 0x3F; có thể đặt LCD_I2C_ADDR qua cờ compiler khi đã biết địa chỉ.

## Danh sách và lịch sử

DanhSach.csv nằm ở gốc thẻ; ví dụ trong docs/examples/DanhSach.csv. UID mẫu là giả, thay bằng UID thật đã gán đúng người.

```csv
uid,student_id,student_name
A1B2C3D4,SV001,NGUYEN VAN A
11223344,SV002,TRAN THI B
```

Tối đa 50 sinh viên; UID HEX HOA có 8/14/20 ký tự (4/7/10 byte), giữ số 0 đầu. ID tối đa 15 ký tự, tên tối đa 48 ký tự ASCII không dấu. Không BOM, dấu phẩy trong trường, ngoặc kép, tên toàn khoảng trắng hoặc dòng trống. UID trùng hoặc file sai/vượt giới hạn bị từ chối toàn bộ. Chấp nhận LF/CRLF và dòng cuối không có newline.

Khi boot/cắm lại SD, firmware đồng bộ danh sách từ Tools/Provision/DanhSach.csv đã nhúng rồi đọc lại; không theo dõi thay đổi file liên tục khi thẻ vẫn đang gắn. history.csv chỉ ghi thêm, gồm uptime_ms,uid,student_id. uptime_ms là thời gian từ khởi động, chưa có RTC/ngày giờ thực. Sai tiêu đề hoặc dòng cuối log dang dở sẽ báo lỗi, không tự xóa/sửa lịch sử. Không rút SD khi đang ghi.

## Kiểm tra trên board

- SD/danh sách/đầu đọc sẵn sàng: LCD hiện MOI QUET THE, LED tắt khi rảnh.
- UID đã đăng ký: chỉ sau khi ghi log thành công mới báo một nhịp và hiện mã/tên khoảng 2 giây.
- Mỗi UID chỉ phát sinh một lượt trong 120 giây. Giữ thẻ liên tục: đủ 120 giây mới nhận lại. Rút rồi đưa lại sớm vẫn bị chặn; thẻ khác có bộ đếm riêng. Xem TICH_HOP_QUAN.md.
- UID chưa đăng ký: ba nhịp, THE KHONG HOP LE, không ghi log.
- SD thiếu/lỗi: LCD báo kiểm tra SD; danh sách thiếu/sai báo LOI DANH SACH. Rút/cắm lại khi không đang ghi để kiểm tra phục hồi.
- RC522 lỗi: LCD báo LOI DAU DOC RFID. Lỗi không được tính là đã rút thẻ; 10 lỗi đọc liên tiếp kích hoạt khởi tạo lại. Khởi tạo thất bại được thử lại sau 500 ms.
- LCD lỗi: thử khởi tạo lại khoảng 2 giây/lần; SD và phản hồi vẫn chạy. Thông báo lỗi nền chỉ hiện LCD, không phát ba nhịp liên tục.
- Nếu PC13 sáng đứng và chương trình dừng, xem g_rtos_fault_file/g_rtos_fault_line bằng debugger để xác định assert/lỗi heap/stack.

Nạp board và thử toàn bộ bằng thẻ thật vẫn là bước bắt buộc. Bản gọn chỉ đã được kiểm tra trong môi trường máy tính; chưa có kết quả nạp board mới.

## Task, queue và RAM

RFID_Read ưu tiên 3, stack 384 words; UI_Feedback ưu tiên 2, stack 384 words; Storage_App ưu tiên 1, stack 768 words; idle stack 128 words. heap_4 cấp phát từ vùng 9 KiB, đã nằm trong BSS. Không cộng lần nữa stack task khi đọc arm-none-eabi-size. MSP/linker stack vẫn 1024 byte, heap C 512 byte.

Queue đầu vào 8 ReaderEvent_t; queue kết quả 4 DisplayEvent_t; mailbox hệ thống 1 phần tử chỉ giữ trạng thái mới nhất. Các dữ liệu được sao chép, không truyền con trỏ mảng danh sách giữa các task. Mỗi ngoại vi có một task sở hữu; không gọi Storage/LCD/Feedback từ task khác hoặc ISR.

Queue đầy thì bên gửi giữ sự kiện và chờ có chỗ. Khi queue đầu vào đầy, RFID tạm ngừng thăm dò, nên không bảo đảm bắt được lượt thẻ đi qua quá nhanh trong thời gian đó. Khi SD chậm, UI vẫn được lập lịch riêng; độ chính xác nhịp thực tế phải thử trên board. Không bảo đảm ghi đúng một lần nếu mất điện giữa thao tác.

SysTick giữ HAL tick và FreeRTOS tick 1 kHz; SVC/PendSV do port ARM_CM3 cung cấp. Giữ đúng luồng này nếu tạo lại code bằng CubeMX: CubeMX có thể tạo lại handler rỗng hoặc sao chép thêm thư viện; phải rà soát diff và build lại, không ghi đè tự động phần tích hợp.

Dùng g_app_rtos_stats/g_rfid_task_stats để xem số lượt đọc/ghi, lỗi, chờ queue, stack/heap còn trống và UID cuối. Đây là snapshot debug, không phải kênh đồng bộ giữa task.
