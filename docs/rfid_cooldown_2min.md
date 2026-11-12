# Chặn quét lặp 2 phút theo từng UID

## Hành vi

Mỗi UID được gửi vào `g_card_event_queue` tối đa một lần trong 120000 ms.
Thời gian bắt đầu được ghi sau khi `xQueueSend` thành công. Gửi thành công
vào queue là mốc của bộ lọc input; kết quả ghi SD do task xử lý của Quyết
quản lý. `CardEvent_t.uptime_ms` vẫn giữ thời gian lúc đọc thẻ ban đầu.

Quét lại trong thời gian chờ không kéo dài mốc hết hạn. Thẻ khác có thời
gian riêng, nên có thể quét ngay. Giữ một thẻ liên tục sẽ tạo tối đa một
sự kiện mỗi 2 phút, kể cả không rút thẻ ra. Task vẫn thăm dò RC522 để đọc
các UID khác.

Bộ lọc nằm trong `Task_RFID_Read`, trước bước gửi queue. Khi queue đầy,
sự kiện được giữ và thử gửi lại như trước; mốc chặn chỉ được ghi sau khi
queue nhận sự kiện. Không gọi hàm ngủ 2 phút cho toàn bộ task.

## Bộ nhớ và giới hạn

Bảng có 50 UID, dùng khoảng 800 byte RAM với TickType_t 32 bit. So sánh
UID theo cả độ dài và từng byte, giữ được byte 00 đầu và UID 4/7/10 byte.
Chỉ dùng lại ô trống hoặc đã hết hạn. Nếu cả 50 ô còn hiệu lực, UID mới
bị từ chối và `cooldown_full` tăng. Lịch sử của 50 UID cũ được giữ để đảm
bảo khoảng chờ của chúng.

Bảng nằm trong RAM và bị xóa khi reset hoặc mất nguồn. Không có lưu bền
thời gian chặn qua reset. Phép so thời gian dùng hiệu số tick không dấu
và đã kiểm thử qua điểm tràn tick. Cấu hình FreeRTOS hiện tại dùng tick
32 bit, tần số 1000 Hz.

## Cấu hình và build trên Ubuntu

Trong `App/Inc/task_rfid_read.h`, thời gian mặc định là:

```c
#define RFID_CARD_COOLDOWN_MS 120000UL
```

Chế độ test cho phép truyền cùng tên biến qua Makefile. Luôn clean khi
đổi tham số build để tránh dùng object của cấu hình trước:

```bash
make RFID_RTOS_TEST=1 clean
make RFID_RTOS_TEST=1 RFID_CARD_COOLDOWN_MS=120000 -j"$(nproc)"
st-flash --reset write build_rfid_rtos/RFIDAttendance.bin 0x08000000
python3 Tests/monitor_rfid_queue.py --port /dev/ttyUSB0
```

Nhấn RESET và kiểm tra có dòng `PER_UID_COOLDOWN_MS=120000`.
`ALIVE` vẫn in heartbeat khoảng mỗi giây. `suppressed` đếm các lần đọc
UID bị chặn, còn `sent` và `rx` đếm sự kiện thực sự gửi/nhận qua queue.

## Kiểm thử trên board cần thực hiện

| Thao tác | Mong đợi | Thực tế |
|---|---|---|
| Quét A lần đầu | Có một QUEUE_RX của A | Chưa thử bản sửa |
| Giữ A hoặc quét lại trước 2 phút | Không thêm QUEUE_RX của A, suppressed tăng | Chưa thử bản sửa |
| Quét B trong lúc A đang chờ | Có một QUEUE_RX của B | Chưa thử bản sửa |
| A đủ 2 phút từ lần gửi queue gần nhất | A được nhận lại | Chưa thử bản sửa |
| Giữ A liên tục hơn 2 phút | Có lần nhận tiếp theo khi đủ thời gian | Chưa thử bản sửa |
| Reset rồi quét A | A được nhận ngay do bảng RAM đã khởi tạo lại | Chưa thử bản sửa |

Thời điểm nhận sau khi hết hạn có thể trễ thêm một chu kỳ quét và thời
gian xử lý/giao nhận queue. Task không đảm bảo đọc mọi thẻ đi ngang nếu
consumer đang nghẽn và queue đã đầy.

## Bài test queue đầy trước đây

Giữ một thẻ sẽ không làm đầy queue nhanh nữa. Để lặp bài kiểm tra nghẽn
10 giây bằng một thẻ, dùng cấu hình thử tắt chặn lặp:

```bash
make RFID_RTOS_TEST=1 clean
make RFID_RTOS_TEST=1 RFID_CARD_COOLDOWN_MS=0 RFID_QUEUE_TEST_PAUSE_MS=10000 -j"$(nproc)"
```

Sau bài test, clean rồi build lại với `RFID_CARD_COOLDOWN_MS=120000` và
`RFID_QUEUE_TEST_PAUSE_MS=0` trước khi dùng chế độ 2 phút.

## Kiểm thử trên máy tính

```bash
python3 Tests/test_rfid_cooldown_host.py
```

Đã kiểm tra bằng GCC với AddressSanitizer/UndefinedBehaviorSanitizer:
lần đầu; mốc 119999/120000 ms; các UID độc lập; quét lặp không gia hạn;
UID 4/7/10 byte và 00 đầu; tick tràn; đủ 50 UID; ô hết hạn; queue đầy;
mốc thời gian bắt đầu sau khi enqueue thành công. Các kiểm thử host
không thay thế việc build ARM và thử thời gian thật trên STM32.
