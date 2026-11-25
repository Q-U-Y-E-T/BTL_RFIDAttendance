# Cách dùng mới: sửa CSV, make, nạp một lần

## 1. File duy nhất cần sửa khi thay danh sách

`Tools/Provision/DanhSach.csv` là đầu vào trên laptop. Giữ tiêu đề chính xác rồi điền thông tin thật:

```csv
uid,student_id,student_name
A1B2C3D4,SV001,NGUYEN VAN A
11223344,SV002,TRAN THI B
```

UID trên chỉ minh họa, phải thay bằng UID thật do RC522 đọc. Gói phát hành để file chỉ có tiêu đề; chưa có dữ liệu sinh viên thì make sẽ báo lỗi, không build firmware chứa danh sách giả/rỗng.

Quy tắc: 1..50 sinh viên; UID HEX HOA có 8/14/20 ký tự, giữ số 0 đầu và không trùng. ID 1..15 ký tự không khoảng trắng; tên 1..48 ký tự ASCII không dấu, không toàn khoảng trắng. Không BOM, dòng trống, ngoặc kép hoặc dấu phẩy trong trường. Được dùng LF/CRLF. Không chỉnh file .h sinh tự động hoặc CSV ví dụ trong docs thay cho file đầu vào này.

## 2. Mỗi lần cập nhật chỉ làm như sau

Mở terminal trong thư mục project mới:

```sh
make -j4
st-flash write build-rtos/RFIDAttendance.bin 0x08000000
```

Chỉ nạp nếu make thành công, rồi nhấn RESET trên board. Không cần make provision, REPLACE_LIST=1 hoặc nạp firmware khác lần thứ hai. Các lệnh chọn firmware riêng cũ đã bị bỏ để tránh nhầm lẫn.

Make luôn kiểm tra CSV trước khi build. CSV được chuyển thành mảng hằng trong Flash; không sinh mã C bằng cách ghép nội dung không kiểm soát. Khi nội dung chuẩn hóa không đổi, header được giữ nguyên để tránh biên dịch lại không cần thiết. Khi dữ liệu đổi, header đổi và firmware được build lại. Mã SHA-256 của dữ liệu hiển thị ở terminal và có trong g_embedded_list_version để debug; MCU đối chiếu nội dung file theo byte.

## 3. Khi khởi động hoặc cắm lại SD

1. main khởi tạo ngoại vi và FreeRTOS. Việc truy cập SD chạy trong task Storage, không chạy sâu trên stack nhỏ của main/ISR.
2. Nếu SD chưa có danh sách, tự tạo từ dữ liệu trong firmware.
3. Nếu file giống hệt dữ liệu nhúng, không ghi lại và không tạo backup mới.
4. Nếu file khác, ghi DSNEW.CSV, đồng bộ/đóng, đọc lại và kiểm tra bằng chính parser Storage. Danh sách cũ được đổi tên thành DSBAK00.CSV, DSBAK01.CSV...; sau đó đưa bản mới vào DanhSach.csv.
5. Chỉ sau khi đồng bộ và đọc danh sách thành công mới đánh dấu sẵn sàng cho điểm danh. Không dùng danh sách đọc dở. Nếu cập nhật lỗi thì báo lỗi và thử khôi phục khi SD hoạt động lại.

Firmware lấy danh sách trong CSV trên laptop làm nguồn chính. Nếu tự sửa CSV trên SD bằng máy khác, firmware sẽ đồng bộ lại theo dữ liệu nhúng ở lần khởi động/cắm lại tiếp theo. Nạp firmware cũ cũng đưa danh sách cũ của firmware đó về SD, có giữ bản trước đó. Cơ chế này so nội dung, không so ngày giờ để tự suy luận bản nào mới hơn.

Không sửa/xóa history.csv trong quá trình cập nhật danh sách. File tạm khác nội dung sẽ được giữ ở TMPBAK00.CSV... thay vì xóa. Tối đa 100 tên mỗi loại backup, không tự ghi đè bản cũ. Nếu hết chỗ backup, ứng dụng báo lỗi danh sách; cần sao lưu/kiểm tra thẻ trước khi dọn các bản cũ.

Giữ nguồn ổn định và không rút SD khi đang ghi. FAT/đổi tên không phải giao dịch chống mất điện hoàn toàn; backup giúp khôi phục nhưng không bảo đảm an toàn trước mọi sự cố nguồn.

## 4. Theo dõi hoạt động

Khi mọi thứ sẵn sàng, LCD hiện MOI QUET THE. Thẻ đã đăng ký: ghi log rồi một nhịp LED; thẻ lạ: ba nhịp. Giữ nguyên thẻ thì không ghi thêm; đưa ra khoảng một giây rồi đưa lại để nhận lượt mới. Ban đầu LCD chưa báo sẵn sàng cho đến khi task Storage có trạng thái thật.

SD lỗi/thiếu: báo kiểm tra SD. Danh sách/cập nhật không hợp lệ hoặc backup đầy: báo lỗi danh sách. Kiểm tra g_list_update_result nếu có debugger; g_embedded_list_version chứa hash dữ liệu đang nhúng. Chưa có RTC, uptime_ms vẫn là thời gian từ lần khởi động. PA8 vẫn dùng LED mô phỏng còi.

## 5. Bộ nhớ và kiểm thử

Chỉ giữ một mảng 50 sinh viên của StorageManager. Bộ cập nhật mượn chính mảng đó để kiểm tra file tạm khi list_ready còn tắt; không cấp thêm mảng 3800 byte. Task Storage vẫn có stack 768 words, UI/RFID 384 words, heap FreeRTOS 9 KiB. Cần kiểm tra số RAM/Flash thực sau build ARM.

```sh
make check
```

26 nhóm core, 21 nhóm định tuyến task/UI và cooldown, 14 nhóm ghi/backup file và 8 nhóm đồng bộ danh sách thật đạt trên máy tính (69 nhóm C). Có thêm kiểm tra generator với 11 nhóm dữ liệu sai và kiểm tra make: dữ liệu giữ nguyên không build lại, dữ liệu đổi sẽ build lại, dữ liệu rỗng bị chặn.

Tests dùng FatFs và code ứng dụng thật với đĩa RAM/HAL/queue có điều khiển, không chứng minh thời gian/context switch/phần cứng ARM. Môi trường sửa code chưa có ARM compiler/board; bản mới cần make và nạp thử trên Ubuntu của Quyết. Không áp dụng số Flash/RAM của bản trước làm số đo mới.
