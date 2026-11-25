# RFIDAttendance — một firmware tự cập nhật danh sách

STM32F103C8T6, HAL, FreeRTOS, RC522/SPI1, MicroSD/SPI2, LCD/I2C1 và LED/còi mô phỏng.

**Chỉ sửa `Tools/Provision/DanhSach.csv`, rồi build và nạp một lần:**

```sh
make -j4
st-flash write build-rtos/RFIDAttendance.bin 0x08000000
```

Nhấn RESET. Khi SD kết nối, task Storage kiểm tra dữ liệu nhúng trong firmware: giống file trên SD thì không ghi lại; khác thì ghi/kiểm tra file tạm, giữ bản danh sách cũ rồi đưa danh sách mới vào dùng. Sau đó ứng dụng chạy điểm danh bình thường. Không còn firmware ghi danh sách riêng và không cần nạp lại bản điểm danh lần thứ hai.

File mẫu chỉ có tiêu đề. Cần điền UID thật và thông tin sinh viên trước khi build. Không đưa UID giả vào firmware. Đọc [hướng dẫn](docs/NAP_DANH_SACH.md).

```sh
make check
```

Bộ kiểm thử máy tính kiểm tra ứng dụng, FIFO task, cập nhật file và cơ chế make sinh dữ liệu. Chưa thay cho build ARM/nạp board. Cần arm-none-eabi-gcc trên Ubuntu; có thể chỉ định GCC_PATH khi cần.

`main.c` chỉ khởi tạo ngoại vi, gọi App_Init() và AppRTOS_Start(); xử lý SD/LCD/RFID nằm trong task riêng. Không tự push/merge repository trong gói này.

Bản này đã tích hợp chống quét lặp 120 giây riêng từng UID của Quân; giữ thẻ liên tục được nhận lại khi hết thời gian. Chi tiết và kiểm thử: `docs/TICH_HOP_QUAN.md`.
