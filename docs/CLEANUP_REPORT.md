# Báo cáo sao lưu và rà soát — 12/11/2026

Báo cáo này mô tả bản gọn trước khi thêm công cụ ghi danh sách. Bản hiện tại có cập nhật danh sách tự động từ Tools/Provision/DanhSach.csv trong cùng firmware; đã bỏ công cụ ghi danh sách hai lần nạp. Có thêm kiểm thử và hướng dẫn tương ứng, không khôi phục demo/test UART cũ.

## Sao lưu trước khi sửa

RFIDAttendance_backup_truoc_don_dep.tar.gz là bản sao byte-for-byte của gói RFIDAttendance_RTOS_don_comment.tar.gz trước khi dọn. SHA-256: `52f2eef471f88813c818f2b81363522dc72f1dcb7f8a2e622a36390611b04c0d`. Không chỉnh bản sao lưu, không xóa hoặc sửa repository trên laptop của Quyết.

## Kết quả rút gọn

| Mục | Trước | Sau |
|---|---:|---:|
| File trong gói nguồn, không gồm build | 1022 | 122 |
| Tổng dung lượng nguồn, xấp xỉ | 66.8 MiB | 3.4 MiB |
| Chế độ firmware | 4 | 1 (FreeRTOS thật) |
| File C biên dịch cho hệ thống thật | 40 | 40 |

Đã loại 871 file thư viện ngoài bộ phụ thuộc hiện tại: CMSIS DSP/NN, thư viện nhị phân và ví dụ cho compiler khác, core/chip khác, template trùng, HAL source/header chưa dùng và các thành phần FreeRTOS không được cấu hình này dùng. Chỉ giữ CMSIS Cortex-M3/GCC/STM32F103xB, HAL đang bật, FatFs, kernel/queue/list/port ARM_CM3/heap_4 và các header mà chúng include trực tiếp hoặc gián tiếp.

Giữ đủ bốn giấy phép CMSIS/CMSIS Device/HAL/FreeRTOS. Các file thư viện còn lại đã được so sánh byte với bản cũ, không sửa nội dung thư viện. Một số header mang tên module chưa bật (ví dụ timers.h) vẫn được giữ vì file kernel include chúng; không suy luận file thừa chỉ từ tên.

Đã bỏ demo thẻ giả, test SD/LCD/RFID thủ công cũ, test UART riêng, patch thử nghiệm, FreeRTOSConfig trong Tests, hai make fragment chọn profile, script/manifest chép bản cập nhật cũ, placeholder và metadata IDE phụ. Tài liệu cũ/trùng được thay bằng README, một hướng dẫn hiện hành và báo cáo này; giữ CSV mẫu để đăng ký sinh viên. Gói không chứa binary/object/cache hoặc bản sao lưu bên trong.

## Rà soát code và build

- Chỉ còn một main và một FreeRTOSConfig sản phẩm. main gọi App_Init/AppRTOS_Start, không chạy xử lý App ở vòng lặp dự phòng sau scheduler.
- Bỏ các nhánh firmware demo/không RTOS và queue chẩn đoán cũ. Không đổi ba task, ưu tiên, stack, heap 9 KiB, số phần tử queue, giao diện module, chân nối, CSV hoặc xử lý điểm danh.
- Makefile chỉ liệt kê 40 nguồn hiện hành, object theo đường dẫn nguồn để không đụng tên. Giữ dependency .d, ELF/map và HEX/BIN; bỏ listing .lst và profile cũ. Tùy chọn cũ bị từ chối thay vì bị bỏ qua âm thầm.
- Dependency closure được lấy từ tiền xử lý toàn bộ 40 file C, cả cấu hình host và macro kiến trúc ARMv7-M. Sau khi cắt thư viện, kiểm tra lại không có include thiếu hoặc C source thừa trong App/Core/Modules/Drivers/Middlewares.
- Đối chiếu cả 40 file C sau tiền xử lý: token tương đương bản RTOS trước khi dọn, có chuẩn hóa tên/số dòng debug và loại trừ hai thay đổi có chủ đích là lời gọi trong vòng lặp main không bao giờ tới và include string.h không dùng ở task RFID.

## Kiểm thử và giới hạn

`make check` đạt 26 nhóm Storage/FatFs/Attendance/Feedback và 12 nhóm định tuyến task/queue. Test core được cập nhật để kiểm tra chặn kết quả chưa tiêu thụ của App RTOS, không còn kiểm tra cách chặn theo feedback của bản vòng lặp cũ.

Kiểm tra thêm: cú pháp C bằng GCC laptop, nguồn/header, chỉ một main, quyền sở hữu ngắt SVC/PendSV/SysTick, heap config và từ chối lựa chọn build cũ. Không phải mô phỏng context switch ARM thật; HAL/queue/thiết bị trong bài host có thành phần thay thế.

Môi trường sửa code chưa có arm-none-eabi-gcc, nên bản gọn chưa được lắp ráp/liên kết ARM hoặc nạp board mới. Bản trước đã được Quyết build ARM thành công (text 33480, data 148, bss 16252 byte); không coi số đó là kết quả đo mới. Phải chạy make clean-all rồi make -j4 trên Ubuntu của Quyết, sau đó thử phần cứng.

Đây là bộ thư viện tối thiểu cho cấu hình hiện tại, không phải bộ SDK đa dòng chip. Khi bật module khác, thay MCU/compiler, hoặc tái tạo bằng CubeMX, cần khôi phục/bổ sung thư viện từ backup/STM32Cube và kiểm tra lại. Không tăng giới hạn RAM/Flash để che lỗi linker.
