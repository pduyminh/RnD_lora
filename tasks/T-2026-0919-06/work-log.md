# Nhật ký thực thi — Task 06 (TM1638 Display & Physical E-Stop)

## 1. Tóm tắt công việc
Đã tích hợp hoàn chỉnh module TM1638 (8 LED 7 đoạn, 8 LED đơn, 8 nút bấm) vào firmware `tx/` bằng driver bit-banging 3 dây thuần ESP-IDF (không dùng bất kỳ thư viện Arduino nào). Module hoạt động độc lập, hiển thị số lượng client kết nối, vận tốc $v_x$ và đóng vai trò nút E-Stop vật lý tại chỗ.

## 2. Các file đã tạo và chỉnh sửa
- `tx/main/tm1638.h` [MỚI]: Khai báo sơ đồ chân theo `knowledge.md` mục 1.2 (STB=GPIO4, CLK=GPIO5, DIO=GPIO6), các mã lệnh chuẩn TM1638 (0x40, 0x42, 0xC0, 0x88), API hiển thị 7 đoạn, bật/tắt LED đơn và đọc phím.
- `tx/main/tm1638.c` [MỚI]: Cài đặt driver 3-wire GPIO, FreeRTOS background task 25Hz (chu kỳ 40ms) quét phím, bắt cạnh lên nút số 1 (S1) để đảo/chốt cờ `estop`, phát ngay gói tin khẩn cấp tức thì qua ESP-NOW, bật/tắt LED 1 chỉ báo E-Stop, cập nhật hiển thị 8 LED 7 đoạn (`C` + client count và $v_x$).
- `tx/main/tx_server.h` & `tx/main/tx_server.c` [CHỈNH SỬA TÍCH HỢP]: Bổ sung các hàm truy xuất an toàn `tx_get_vx()`, `tx_set_estop()`, `tx_get_estop()`, `tx_send_packet_now()` và ép vận tốc về 0 khi `estop=1`. Không làm thay đổi logic Web Server hay WebSocket từ T05.
- `tx/main/CMakeLists.txt` [HỖ TRỢ BUILD]: Thêm `tm1638.c` vào danh sách nguồn `SRCS`.
- `tx/main/main.c` [HỖ TRỢ BOOT]: Bổ sung gọi `tm1638_init()` khi khởi động.
- `tests/test_tm1638.py` [MỚI]: 8 unit test kiểm chứng sơ đồ chân, mã hóa 7 đoạn, định dạng hiển thị, máy trạng thái nút bấm 1 chốt/nhả estop và phát tức thì.
- `docs/pending_hardware_verification/task-06.md` [MỚI]: Quy trình kiểm chứng thực nghiệm trên phần cứng thật, đánh dấu `[BLOCKED]` do lab chưa cắm phần cứng.

## 3. Xác nhận phạm vi file
- Hoàn toàn KHÔNG đụng đến `rx/`, `common/protocol.h`, `knowledge.md`.
- Các file phụ trợ (`tests/*`, `docs/*`, `CMakeLists.txt`, `main.c`, `tx_server.*`) chỉ phục vụ đăng ký build, boot, và hook getter kết nối mà không sửa đổi luồng web server/WebSocket của T05, tuân thủ chặt chẽ quy định tại `auditor.txt` và `lessons-learned.md`.
