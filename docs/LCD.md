# LCD 16x2 I2C – phần của Trần Ngọc Sơn

Theo đề cương nhóm 07, Sơn phụ trách khối Output: LCD 16x2 nối qua I2C, hiển thị tên, mã học sinh và trạng thái điểm danh ngay khi quét thẻ. Code dùng thư viện HAL (I2C1, `hi2c1` do `MX_I2C1_Init` cấu hình).

## File

| File | Vai trò |
|---|---|
| `Modules/LCD/lcd.h` | Hợp đồng hàm chung của nhóm (không sửa) |
| `Modules/LCD/lcd.c` | Driver HD44780 qua PCF8574: `LCD_Init`, `LCD_ShowEvent` |
| `Modules/LCD/lcd_display.h/.c` | Quản lý hiển thị cho vòng lặp chính: hiện kết quả, giữ 3 s rồi về `MOI QUET THE`, tự phục hồi khi LCD lỗi |
| `Core/Src/main.c` | Thêm 3 chỗ trong vùng USER CODE: include, `Display_Init`, lấy `App_TakeDisplayEvent` đưa vào `Display_Show` |
| `Tests/attendance_demo.c` | Bỏ 3 lời gọi `App_TakeDisplayEvent`, để kết quả demo đi qua vòng lặp chính tới LCD |
| `Tests/host/lcd/`, `Tests/run_lcd_host_tests.py` | Kiểm thử trên máy tính với LCD giả lập và App của Quyết |
| `Makefile` | Thêm `lcd.c`, `lcd_display.c`; `make test-host` chạy thêm test LCD |

## Ghép với App (Quyết)

```c
App_Init();
Display_Init(HAL_GetTick());                 /* hiện MOI QUET THE */
while (1) {
    App_Process();
    DisplayEvent_t display;
    if (App_TakeDisplayEvent(&display)) {    /* luôn lấy, kể cả khi LCD lỗi */
        Display_Show(&display, HAL_GetTick());
    }
    Display_Process(HAL_GetTick());
    HAL_Delay(5);
}
```

Vòng lặp luôn lấy kết quả của App, nên App không bị kẹt `ATTENDANCE_BUSY`, kể cả khi LCD hỏng. Mọi hàm LCD chỉ được gọi từ vòng lặp này, vì đây là nơi duy nhất dùng I2C1.

## Hiển thị

| Kết quả từ App | Dòng 1 | Dòng 2 | Thời gian |
|---|---|---|---|
| Khởi động / chờ | `MOI QUET THE` | (trống) | Đến khi có lượt quét |
| `APP_CARD_ACCEPTED` | Mã học sinh | Tên (cắt còn 16 ký tự) | 3 s rồi về chờ |
| `APP_CARD_UNKNOWN` | `THE KHONG HOP LE` | `VUI LONG THU LAI` | 3 s |
| `APP_STORAGE_ERROR` | `LOI LUU DU LIEU` | `KIEM TRA THE SD` | 3 s |
| `APP_LIST_ERROR` | `LOI DANH SACH` | `KIEM TRA FILE` | 3 s |

- App của Quyết chỉ báo lỗi khi có lượt quét, nên mỗi lỗi cũng được coi là phản hồi cho một lần quét và hiện 3 s. Cùng thẻ bị chặn lặp thì App không gửi kết quả, LCD giữ nguyên.
- Kết quả mới thay ngay kết quả cũ, không sót chữ (mỗi dòng luôn ghi đủ 16 ô), và tính lại mốc 3 s.
- Ký tự ngoài ASCII hiện thành `?`. Chuỗi thiếu `'\0'` vẫn an toàn.
- **LCD lỗi** (rút dây, nhiễu): cứ 0,5 s thử lại một lần. Mỗi lần thử, task reset I2C1 (`HAL_I2C_DeInit` rồi `HAL_I2C_Init`, có SWRST) để gỡ lỗi kẹt cờ BUSY của STM32F1, rồi khởi tạo lại LCD và hiện lại nội dung đang cần hiện. Vòng lặp không treo.
- `LCD_Init` chờ khoảng 60 ms bằng `HAL_Delay` (chỉ chạy lúc khởi động hoặc khi phục hồi). Nếu build với `-DLCD_USE_FREERTOS`, khi scheduler đang chạy nó dùng `vTaskDelay`. Mọi lần truyền I2C có timeout 20 ms.
- Thông số chỉnh trong `lcd_display.h`: `LCD_DISPLAY_HOLD_MS` (3000), `LCD_DISPLAY_RETRY_MS` (500). Có thể xem bộ đếm `g_display_stats` bằng debugger.

## Phần cứng

LCD 1602 có module I2C PCF8574. Driver dò lần lượt địa chỉ `0x27` rồi `0x3F`.

| LCD (module I2C) | Nối với |
|---|---|
| SDA | PB7 |
| SCL | PB6 |
| GND | G |
| VCC | 5V (LCD 1602 cần 5V để chữ rõ) |

PB6 và PB7 chịu được 5V. Không cần điện trở ngoài. Nếu không thấy chữ, vặn biến trở xanh trên module.

## Kiểm thử

### Trên máy tính

```sh
make test-host
```

Lệnh này chạy test của Quyết (26 nhóm) rồi chạy test LCD (18 ca). Test LCD dùng LCD 1602/PCF8574 giả lập, giải mã từng byte I2C như chip thật, chạy kèm AddressSanitizer và UBSan:

- **Phần `lcd_display.c` (ca 1–10):**
  - đủ 5 trạng thái;
  - giữ 3 s rồi về màn hình chờ;
  - cắt tên dài, kết quả mới thay ngay;
  - từ chối NULL hoặc status sai;
  - chuỗi thiếu `'\0'`;
  - không có giao dịch I2C thừa khi đứng yên;
  - rút LCD kèm I2C kẹt BUSY, cắm lại thì tự phục hồi.
- **Phần ghép với App của Quyết (ca 11–18)**, dùng FatFs thật trên đĩa RAM và chạy đúng vòng lặp của `main.c`:
  - bật nguồn hiện `MOI QUET THE`;
  - quét SV001 thì hiện mã và tên;
  - giữ thẻ thì không hiện lại;
  - thẻ lạ thì báo `THE KHONG HOP LE`;
  - thẻ UID 7 byte;
  - LCD hỏng thì App vẫn nhận lượt quét tiếp theo;
  - cắm lại LCD thì hiện kết quả mới nhất;
  - rút thẻ SD thì LCD báo lỗi.

### Trên board

1. **Bản thường:** chạy `make -j4`, nạp `build/RFIDAttendance.bin` (hoặc `.hex`). Khi bật nguồn, LCD phải hiện `MOI QUET THE`. Đầu đọc RFID chưa được ghép vào `develop`, nên chưa quét thẻ thật được.
2. **Bản demo của Quyết (cần thẻ SD có `DanhSach.csv` hợp lệ):** chạy `make DEMO=1 -j4`, nạp `build-demo/RFIDAttendance.bin`. Khoảng 2 s sau khi bật, LCD hiện mã và tên của học sinh đầu tiên trong danh sách. Sau 3 s về `MOI QUET THE`. Khoảng 3 s sau lần đầu, LCD hiện `THE KHONG HOP LE` / `VUI LONG THU LAI`, rồi về `MOI QUET THE`.

## Kết quả test phần cứng

| Ca | Mong đợi | Kết quả |
|---|---|---|
| Bật nguồn | `MOI QUET THE` | Đạt (Sơn, bản test FreeRTOS, 12/11) |
| Demo có SD | Tên học sinh đầu tiên, rồi `THE KHONG HOP LE` | Chưa kiểm tra |
| Rút/cắm dây SDA | LCD tự hiện lại | Chưa kiểm tra |
| Địa chỉ I2C thực tế | 0x27 hoặc 0x3F | Chưa ghi |

## Khi chuyển sang FreeRTOS

Nhánh `feature/rfid-task-queue` của Quân dùng FreeRTOS. Sơn đã có sẵn bản `Task_LCD_Display`: nhận `DisplayEvent_t` qua queue 1 phần tử ghi đè, đã test trên board ở chế độ hiển thị `MOI QUET THE`. Bản này sẽ được đưa lên khi nhóm ghép FreeRTOS vào `develop`.
