# Task đọc RFID và queue của Nguyễn Hồng Quân

## Luồng dữ liệu

`Task_RFID_Read` sở hữu RC522/SPI1, gọi `RFID_ReadCard`, rồi gửi toàn bộ
`CardEvent_t` vào `g_card_event_queue`. Queue có 8 phần tử, sao chép struct
gồm `uid[10]`, `uid_len` và `uptime_ms` theo header chung `app_types.h`.

`Task_Queue_TestReceiver` nhận bằng `xQueueReceive` và in UID ra UART.
Khi tích hợp, task xử lý của Quyết nhận queue này thay task test. Một queue
có một consumer xử lý điểm danh để tránh các task tranh lấy cùng sự kiện.

## File chính

- `App/Inc/app_queues.h`, `App/Src/app_queues.c`: tạo queue chung trước task.
- `App/Inc/task_rfid_read.h`, `App/Src/task_rfid_read.c`: task quét và gửi queue.
- `Modules/RC522/rfid_delay.h`: dùng `vTaskDelay` khi scheduler đang chạy,
  dùng HAL khi chạy firmware nền hoặc trước scheduler.
- `App/rfid_task_queue.mk`: bật profile thử bằng `RFID_RTOS_TEST=1`.
- `Tests/FreeRTOSConfig.h`: cấu hình thử của FreeRTOS V10.5.1, heap_4 8 KiB,
  tick 1 kHz, Cortex-M3. Nguồn kernel và MIT license đi kèm.
- `Tests/main_rfid_queue_test.c`: điểm vào của firmware test và consumer UART.
- `Tests/freertos_test_interrupts.c`: SysTick gọi HAL và FreeRTOS.

Profile thử chọn test main thay `Core/Src/main.c`, chọn test SysTick thay
`Core/Src/stm32f1xx_it.c`. SVC và PendSV do port Cortex-M3 xử lý. Mỗi firmware
chỉ liên kết một main và một bộ handler. Build mặc định giữ cấu hình nền.

## Build và nạp trên Ubuntu

Chạy trong `BTL_RFIDAttendance`:

```bash
make RFID_RTOS_TEST=1 -j"$(nproc)"
st-flash --reset write build_rfid_rtos/RFIDAttendance.bin 0x08000000
python3 Tests/monitor_rfid_queue.py --port /dev/ttyUSB0
```

UART: PA9 -> RX USB-UART mức 3.3 V, nối chung GND, 115200-8N1.
Đóng các monitor khác trước khi mở cổng. Nhấn RESET sau khi mở monitor.

Mong đợi:

```text
RFID_TASK_QUEUE_TEST V1 115200-8N1
Queue and tasks created; starting scheduler.
ALIVE init=0 ...
QUEUE_RX #1 UID=B369A2A7 len=4 uptime_ms=... sent=1 waits=0
```

UID trong QUEUE_RX lấy từ bản sao đã nhận trong queue. Consumer không gọi
driver RFID. LED PC13 nháy heartbeat để cho biết consumer vẫn chạy.

## Hành vi và kiểm thử phần cứng

Task ưu tiên 2, stack 384 words, chu kỳ mục tiêu 50 ms. Consumer test ưu tiên 1,
stack 512 words. Nếu một lần xử lý vượt chu kỳ, task chờ thêm trước lần đọc
tiếp theo để tránh quét dồn. Lỗi khởi tạo được đếm và thử lại sau 500 ms.
Lỗi đọc được đếm; dữ liệu chỉ gửi khi driver trả RFID_OK.

Giữ thẻ có thể tạo nhiều sự kiện cùng UID. Quyết xử lý chống điểm danh lặp
ở tầng ứng dụng. `uptime_ms` là HAL tick từ lúc chạy, bắt đầu lại sau reset.

Khi queue đầy, task giữ sự kiện hiện tại và chờ chỗ trống theo từng đợt
100 ms; biến `queue_waits` tăng khi hết một đợt chờ. Task tạm dừng quét mới
trong thời gian này. Thẻ đi ngang trong lúc bị nghẽn có thể chưa được đọc.
Nhóm cần chốt dung lượng queue và phản hồi bận khi tích hợp toàn hệ thống.

Các ca cần điền kết quả thật:

| Ca | Mong đợi | Kết quả trên board |
|---|---|---|
| Không thẻ 10 giây | ALIVE tăng, không có QUEUE_RX mới | Chưa kiểm tra |
| Một thẻ | QUEUE_RX chứa đúng UID và độ dài | Chưa kiểm tra |
| Thẻ khác | Consumer nhận UID khác | Chưa kiểm tra |
| Rút và đưa lại | Tiếp tục nhận UID, không treo | Chưa kiểm tra |
| Reset | Tạo queue/task lại và đọc được | Chưa kiểm tra |
| Queue đầy | waits tăng, consumer chạy lại thì giao được sự kiện chờ | Chưa kiểm tra |
| Bộ nhớ | heap và stack watermark còn dương với biên đủ cho tích hợp | Chưa kiểm tra |

## Thử queue đầy

```bash
make RFID_RTOS_TEST=1 clean
make RFID_RTOS_TEST=1 RFID_QUEUE_TEST_PAUSE_MS=10000 -j"$(nproc)"
st-flash --reset write build_rfid_rtos/RFIDAttendance.bin 0x08000000
```

Mở monitor, nhấn RESET và giữ thẻ ngay trong 10 giây đầu. Consumer test
tạm ngủ 10 giây để queue đầy. Sau dòng `receiver resumed`, dữ liệu được
nhận và `waits` cho thấy producer đã chờ. Bỏ thẻ ra, chờ queue rỗng rồi
so sánh `sent` và `rx`. Trở về bản test thường bằng clean và build lại
với `RFID_RTOS_TEST=1` không truyền tham số pause.

## Bàn giao và tích hợp

Kernel được lấy nguyên từ gói RC522 của nhóm, V10.5.1, MIT license.
Nguồn chính thức: https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V10.5.1

Quyết duyệt cấu hình kernel, stack/heap, ưu tiên và tick khi ghép SD/LCD.
Profile hiện tại dành cho kiểm thử input và queue. Bật Software Timer cho
buzzer hoặc thêm task xử lý sẽ cần cấu hình bộ nhớ chung tương ứng.

Trong main tích hợp, sau HAL/GPIO/SPI đã khởi tạo: gọi App_Queues_Init,
RFID_Task_Start, tạo task xử lý nhận g_card_event_queue, rồi mới gọi
vTaskStartScheduler. Dùng một kernel, một heap và một bộ exception handler.
Đưa các nguồn App và include/define từ profile vào cấu hình build tích hợp
do nhóm quản lý. Task nhận LCD dùng queue kết quả do tầng xử lý cung cấp.

Sau khi test thật, cập nhật bảng kết quả, đính kèm log và PR nhánh task/queue.
Queue day: DAT.
Consumer tam dung 10 giay, queue_waits tang den 94.
Sau khi consumer chay lai: sent=29, rx=29.
Khong ghi nhan loi khoi tao/doc trong doan test nay.
