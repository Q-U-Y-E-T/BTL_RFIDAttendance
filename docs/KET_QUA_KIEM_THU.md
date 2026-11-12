# Kết quả kiểm thử — 12/11/2026

## Đã chạy trong môi trường sửa code

- `make test-host`: đạt 26 nhóm kiểm thử. Dùng Storage/Attendance/App/Feedback/StorageManager thật và FatFs thật; HAL và SD được thay bằng mô hình đĩa RAM FAT16.
- AddressSanitizer và UndefinedBehaviorSanitizer bật; không phát hiện lỗi trong các ca đã chạy. Tắt LeakSanitizer để tương thích môi trường chạy có ptrace; bộ test không cấp phát heap.
- `make -n` và `make -n DEMO=1`: đạt kiểm tra chọn nguồn, macro và thư mục đầu ra khác nhau. Đây là kiểm tra lệnh build, không phải build ARM.
- Kiểm tra cú pháp App/demo bằng GCC trên máy tính: đạt.

## Chưa xác minh cho bản sửa mới

Môi trường hiện tại không có arm-none-eabi-gcc. Chưa biên dịch/liên kết firmware ARM, chưa đo Flash/RAM bản mới và chưa nạp board. Cần chạy `make clean-all`, `make -j4`, `make DEMO=1 -j4` trên Ubuntu của Quyết rồi thử phần cứng theo hướng dẫn.

Các lần thử LED/SD thành công trước đó do Quyết xác nhận thuộc bản trước. Test đĩa RAM không chứng minh chất lượng nguồn, SPI, rút nóng hay driver RFID thật. Chưa có driver RFID/LCD, FreeRTOS/queue và còi thật trong kiểm thử này.

## Kết quả chi tiết

```text
python3 Tests/run_host_tests.py
PASS 01 missing CSV returns error and count=0
PASS 02 real FatFs reads CSV, 4/7/10-byte UIDs and leading zero
PASS 03 capacity and invalid arguments
PASS 04 11 malformed CSV cases rejected
PASS 05 51 students rejected without truncation
PASS 06 overlong student name rejected
PASS 07 embedded NUL rejected
PASS 08 LF and last line without newline accepted
PASS 09 append preserves prior log and creates header only once
PASS 10 UID mismatch and NULL do not write log
PASS 11 partial final log line rejected without append
PASS 12 wrong history header rejected
PASS 13 disk write/sync failure never reports success
PASS 14 public card input publishes one successful display event
PASS 15 held UID never logs again, even as time advances
PASS 16 short false no-card result does not rearm
PASS 17 long gap without polling does not prove removal
PASS 18 reader error is not treated as removal
PASS 19 stable absence followed by re-presentation permits new log
PASS 20 unknown UID publishes error once and never logs
PASS 21 write failure invalidates SD and never automatically retries same card
PASS 22 remove/reinsert replaces list with changed CSV
PASS 23 invalid replacement CSV never becomes active list
PASS 24 corrected CSV recovers without MCU reset
PASS 25 absence timing survives HAL tick wrap
PASS 26 feedback polarity, one/three pulses and busy rejection
ALL 26 CASE GROUPS PASSED
```
