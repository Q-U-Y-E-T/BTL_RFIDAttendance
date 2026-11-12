# RFIDAttendance — phần SD, điểm danh và phản hồi của Quyết

STM32F103C8T6, HAL, Makefile. Bản mặc định không tạo lượt điểm danh giả.

Đọc [hướng dẫn](docs/HUONG_DAN_PHAN_QUYET.md) trước khi nạp; xem [kết quả kiểm thử](docs/KET_QUA_KIEM_THU.md).

```sh
make test-host       # GCC + Python 3 trên Ubuntu, không cần board
make clean-all
make -j4            # cần arm-none-eabi-gcc; đầu ra build/
```

`make DEMO=1 -j4` tạo bản thử thẻ giả trong `build-demo/`. Mỗi lần reset bản demo có thể thêm một dòng log giả vào SD. Bản mặc định không tự gọi driver RFID/LCD; ghép các module đó sau qua `App_SubmitCard()` và `App_TakeDisplayEvent()`.

Chưa tích hợp FreeRTOS, queue hoặc RTC. Còi đang được mô phỏng bằng LED ngoài PA8.
