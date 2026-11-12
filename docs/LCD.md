# Module LCD 16x2 I2C

Người phụ trách: Trần Ngọc Sơn. Nhánh: `feature/lcd-driver`

## Các file

| File | Vai trò |
|------|---------|
| `Modules/LCD/lcd.h` | Header chung đã thống nhất. File này **không sửa**. |
| `Modules/LCD/lcd.c` | Driver HD44780 qua bộ mở rộng I2C PCF8574 |
| `Tests/test_lcd.c` | Test thủ công trên board. File này không nằm trong build mặc định. |

Module không dùng thư viện ngoài nào, chỉ dùng HAL I2C có sẵn và `hi2c1` lấy từ `i2c.h`.

## Phần cứng và đấu dây

LCD 1602 có gắn module I2C (chip PCF8574T hoặc PCF8574AT).

| Chân module I2C | Nối với STM32F103 |
|-----------------|-------------------|
| GND | GND |
| VCC | 5V (LCD 1602 cần 5V để hiện rõ chữ) |
| SDA | PB7 (I2C1_SDA) |
| SCL | PB6 (I2C1_SCL) |

- PB6 và PB7 là chân chịu được 5V (FT), nên có thể dùng điện trở kéo lên 5V có sẵn trên module I2C.
- Module I2C phải có jumper đèn nền. Chữ không rõ hoặc chỉ thấy các ô đen thì vặn biến trở xanh trên module.
- Driver giả định module nối chân theo kiểu phổ biến: P0=RS, P1=RW, P2=EN, P3=đèn nền, P4..P7=D4..D7.

## Địa chỉ I2C

Lúc khởi tạo, driver dò lần lượt địa chỉ 7-bit `0x27` (PCF8574T) rồi `0x3F` (PCF8574AT). Nếu đã biết địa chỉ thật, có thể cố định nó bằng cách thêm `C_DEFS += -DLCD_I2C_ADDR=0x27` vào Makefile. Sau khi test trên board, hãy ghi địa chỉ thực tế vào mục **Kết quả** bên dưới.

## Hành vi

| Trạng thái | Dòng 1 | Dòng 2 |
|------------|--------|--------|
| `APP_READY` | `MOI QUET THE` | (trống) |
| `APP_CARD_ACCEPTED` | `student_id` | `student_name`, cắt còn 16 ký tự |
| `APP_CARD_UNKNOWN` | `THE KHONG HOP LE` | `VUI LONG THU LAI` |
| `APP_STORAGE_ERROR` | `LOI LUU DU LIEU` | `KIEM TRA THE SD` |
| `APP_LIST_ERROR` | `LOI DANH SACH` | `KIEM TRA FILE` |

- Mỗi dòng luôn được ghi đủ 16 ô và phần dư được đệm bằng dấu cách, nên không còn sót chữ cũ. Driver không dùng lệnh Clear, nên màn hình không nháy và không phải chờ 1,52 ms.
- `LCD_ShowEvent` chỉ đọc dữ liệu từ `*event`. Hàm không sửa dữ liệu và không giữ lại con trỏ. Nó đọc tối đa `sizeof` của mỗi mảng, nên không đọc tràn kể cả khi chuỗi thiếu `'\0'`.
- Ký tự nằm ngoài ASCII in được (ví dụ chữ có dấu UTF-8) được hiển thị thành `?`.
- `LCD_ERROR` được trả về trong các trường hợp sau: `event == NULL`, chưa gọi `LCD_Init` hoặc gọi Init bị lỗi, `status` không hợp lệ (khi đó màn hình giữ nguyên nội dung), hoặc truyền I2C lỗi.
- Mọi lần truyền I2C đều có timeout 20 ms. Một lần hiển thị gồm 2 lần truyền, mỗi lần 68 byte, mất khoảng 6 ms ở tốc độ 100 kHz.
- `LCD_ShowEvent` không dùng `HAL_Delay`, nên sau này chạy được trong task FreeRTOS. Riêng `LCD_Init` có dùng `HAL_Delay` (khoảng 60 ms), vì vậy nên gọi nó trước khi khởi động scheduler.
- Khi `LCD_ShowEvent` báo lỗi, tầng ứng dụng có thể gọi lại `LCD_Init()` để khôi phục, ví dụ sau khi LCD bị rút ra rồi cắm lại. Vì LCD không thể tự báo lỗi của chính nó, hãy dùng LED hoặc debugger để chẩn đoán.

## Chạy test trên board (thao tác tạm, không commit)

1. Trong `Makefile`, thêm dòng sau vào ngay dưới dòng `C_SOURCES += Modules/LCD/lcd.c`:
   ```make
   C_SOURCES += Tests/test_lcd.c
   ```
2. Trong `Core/Src/main.c`:
   ```c
   /* USER CODE BEGIN PFP */
   uint32_t Test_LCD_Run(void);
   /* USER CODE END PFP */

   /* USER CODE BEGIN 2 */
   uint32_t lcd_fail = Test_LCD_Run();
   /* USER CODE END 2 */

   /* USER CODE BEGIN 3 */
   HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
   HAL_Delay(lcd_fail == 0U ? 1000U : 100U);   /* chậm = đạt, nhanh = lỗi */
   /* USER CODE END 3 */
   ```
   Thao tác này thay cho 2 dòng nháy LED cũ trong `USER CODE 3`.
3. Chạy `make clean && make -j"$(nproc)"`, rồi nạp `build/RFIDAttendance.bin`.
4. Quan sát LCD. Mỗi bước giữ màn hình khoảng 2,5 giây:

| Bước | Màn hình mong đợi |
|------|-------------------|
| 1 | Init (chưa có chữ) |
| 2 | Kiểm tra NULL (không đổi) |
| 3 | `MOI QUET THE` / trống |
| 4 | `HS001` / `NGUYEN VAN A` |
| 5 | `THE KHONG HOP LE` / `VUI LONG THU LAI` |
| 6 | `LOI LUU DU LIEU` / `KIEM TRA THE SD` |
| 7 | `LOI DANH SACH` / `KIEM TRA FILE` |
| 8 | `B23DCDT225ABCDE` / `TRAN NGOC SON TE` (tên dài bị cắt) |
| 9 | `HS2` / `AN` (không còn sót chữ của bước 8) |
| 10 | Trạng thái sai: màn hình vẫn là `HS2` / `AN` |
| 11 | `MOI QUET THE` / trống |

Sau đó LED PC13 nháy chậm (1 giây) nghĩa là test đạt, nháy nhanh (0,1 giây) nghĩa là có lỗi. Khi có lỗi, xem biến `g_test_lcd_first_fail_step` bằng debugger để biết bước nào lỗi đầu tiên. Nếu bước 1 lỗi, hãy kiểm tra dây, nguồn và địa chỉ I2C.

5. **Trước khi commit**, bỏ dòng `Tests/test_lcd.c` khỏi Makefile, trả `main.c` về như cũ (`git checkout Core/Src/main.c`), rồi build lại.

## Đã kiểm tra

- Kiểm tra mô phỏng trên PC: driver chạy với bộ giả lập PCF8574 và HD44780 (giải mã từng byte I2C trên sườn xuống của EN). Kết quả là 27/27 ca đạt, gồm: đủ 5 trạng thái, cắt tên dài, ghi nội dung ngắn sau nội dung dài, chuỗi thiếu `'\0'`, ký tự UTF-8, lỗi I2C, dò địa chỉ 0x3F, Init lại khi LCD đang ở chế độ 4 bit và lệch nhịp, và trường hợp không có LCD.
- `lcd.c` và `test_lcd.c` đã được kiểm tra cú pháp với header HAL của project.
- **Chưa build bằng arm-none-eabi-gcc và chưa test trên phần cứng thật.**

## Kết quả test phần cứng (điền sau khi test)

- Commit đã test:
- Board / module LCD / chip I2C:
- Địa chỉ I2C thực tế:
- Nguồn cấp LCD:
- Kết quả 11 bước:
- Ghi chú:
