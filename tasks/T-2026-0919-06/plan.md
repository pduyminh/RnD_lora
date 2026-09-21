---
role: codex-advisor
task_id: T-2026-0919-06
round: 1
---

# Khuyến nghị thực thi: Task 06 — TX TM1638 Display & Physical E-Stop

## Cách tiếp cận đề xuất
1. Cài đặt module TM1638 (8 LED 7 đoạn, 8 LED đơn, 8 nút bấm) thuần ESP-IDF (không dùng Arduino) trong `tx/main/tm1638.h` và `tx/main/tm1638.c`.
2. Sơ đồ chân theo mục 1.2 trong `knowledge.md`:
   - STB: GPIO 4
   - CLK: GPIO 5
   - DIO: GPIO 6 (chuyển đổi hai chiều INPUT có pull-up khi đọc phím và OUTPUT khi gửi lệnh/dữ liệu).
3. Giao thức bit-banging 3 dây chuẩn TM1638:
   - Các lệnh chuẩn: `0x40` (ghi tự tăng địa chỉ), `0x42` (đọc phím quét), `0xC0` (địa chỉ RAM bắt đầu), `0x88 | 0x07` (bật hiển thị độ sáng cực đại).
   - Hàm đọc 4 byte quét phím `tm1638_read_keys()` ánh xạ về bitmap 8 nút bấm (S1 đến S8).
4. Xử lý nút bấm số 1 (S1) làm E-Stop vật lý:
   - Cơ chế phát hiện cạnh lên (rising edge) và lọc dội phím (debounce).
   - Bấm nút 1 lần 1: chốt cờ `estop = 1`, bật sáng LED đơn 1 (LED đỏ đầu tiên trên module).
   - Phát ngay lập tức 1 gói tin `ctrl_packet_t` có `estop=1` qua ESP-NOW mà không cần chờ hết chu kỳ 20ms.
   - Duy trì trạng thái estop (kể cả khi các lệnh joystick từ web tiếp tục gửi) cho tới khi bấm lại nút 1 lần 2.
   - Bấm nút 1 lần 2: hủy bỏ chốt estop (`estop = 0`), tắt LED đơn 1.
5. Hiển thị thông tin lên 8 LED 7 đoạn:
   - Digits 0-1: Ký tự `'C'` kết hợp số client AP/WebSocket đang kết nối (ví dụ: `C0`, `C1`, `C2`, ...).
   - Digit 2: Ký tự trống `' '` làm khoảng phân tách.
   - Digits 3-7: Giá trị vận tốc $v_x$ hiện tại định dạng số nguyên mm/s (ví dụ `  500`, `- 250`, `    0`).
6. Tích hợp và kiểm chứng:
   - Bổ sung các getter/setter an toàn `tx_get_vx()`, `tx_set_estop()`, `tx_get_estop()`, `tx_send_packet_now()` trong `tx/main/tx_server.h` và `tx/main/tx_server.c` để kết nối TM1638 với luồng phát truyền thông mà không sửa đổi logic Web Server/WebSocket.
   - Cập nhật `tx/main/CMakeLists.txt` đăng ký `tm1638.c` vào danh sách nguồn `SRCS`.
   - Cập nhật `tx/main/main.c` gọi `tm1638_init()` lúc khởi động hệ thống.
   - Viết bộ unit test `tests/test_tm1638.py` (8 test) kiểm tra sơ đồ chân, bảng mã lệnh, mã hóa 7 đoạn, định dạng hiển thị, máy trạng thái nút bấm 1 E-Stop chốt & gửi ngay, và kiểm tra tích hợp mã nguồn.
   - Lập tài liệu kiểm chứng phần cứng còn chờ `docs/pending_hardware_verification/task-06.md` [BLOCKED].

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `tx/main/tm1638.h`, `tx/main/tm1638.c`, `tx/main/tx_server.h`, `tx/main/tx_server.c` (chỉ thêm hook getter/setter kết nối, không sửa web server), `tx/main/CMakeLists.txt`, `tx/main/main.c`, `tests/test_tm1638.py`, `docs/pending_hardware_verification/task-06.md`, các file nhật ký trong `tasks/T-2026-0919-06/`.
- CẤM sửa `rx/`, `common/protocol.h`, `knowledge.md`, `task.md`.
- Sử dụng ESP-IDF thuần, không dùng Arduino libraries.
- Các tiêu chí kiểm chứng nút bấm vật lý và quan sát LED trên module thật được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-06.md` đánh dấu `[BLOCKED]`.

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- `idf.py -C tx build` trả exit code 0.
- `idf.py -C rx build` trả exit code 0.
- `python -m pytest tests -v --tb=short` pass 100% (39/39 tests).
- Module TM1638 hỗ trợ đầy đủ hiển thị AP client count, vận tốc $v_x$, LED 1 báo estop, và nút bấm 1 chốt/nhả estop kèm gửi gói khẩn cấp tức thì.
- Có tài liệu kiểm chứng phần cứng [BLOCKED] cho task-06.
