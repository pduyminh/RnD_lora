# Nhật ký thực thi — Task 07 (TX Telemetry Reception & Dual-Display Alert)

## 1. Tóm tắt công việc
Đã hoàn thành module tiếp nhận bản tin `telemetry_packet_t` từ RX gửi sang TX qua ESP-NOW. Hệ thống kiểm tra tính toàn vẹn (magic `0x5A` và CRC-16/CCITT-FALSE), giám sát ngưỡng timeout 500ms mất kết nối, đồng thời cảnh báo thời gian thực song song lên cả hai giao diện:
- Đèn LED đơn thứ 2 trên module TM1638 (sáng khi mất kết nối, tắt khi bình thường).
- Banner trạng thái trên giao diện web điện thoại qua WebSocket (chuyển đỏ "MẤT KẾT NỐI" và xanh "OK" không cần refresh).

## 2. Các file đã tạo và chỉnh sửa
- `tx/main/telemetry.h` [MỚI]: Khai báo API tiếp nhận telemetry, kiểm tra timeout 500ms (`TELEMETRY_TIMEOUT_MS`), truy vấn trạng thái liên kết `telemetry_is_link_ok()`.
- `tx/main/telemetry.c` [MỚI]: Cài đặt callback nhận ESP-NOW, xử lý và xác thực gói tin (magic + CRC), FreeRTOS background task 50ms kiểm tra timeout, điều khiển LED 2 TM1638 và phát thông báo WebSocket tới trình duyệt.
- `tx/main/tx_server.h` & `tx/main/tx_server.c` [HỖ TRỢ TÍCH HỢP]: Bổ sung hàm `tx_broadcast_ws_message()` gửi thông điệp JSON bất đồng bộ tới toàn bộ client WebSocket đang kết nối; cập nhật `INDEX_HTML` bổ sung banner `#rx-status` và lắng nghe `ws.onmessage`.
- `tx/main/tm1638.c` [CHỈNH SỬA TÍCH HỢP]: Bổ sung lời gọi `tm1638_set_led(1, !telemetry_is_link_ok())` trong task TM1638 để cập nhật LED 2 độc lập.
- `tx/main/main.c` [HỖ TRỢ BOOT]: Bổ sung gọi `telemetry_init()` trong luồng khởi động `app_main`.
- `tx/main/CMakeLists.txt` [HỖ TRỢ BUILD]: Thêm `telemetry.c` vào danh sách nguồn `SRCS`.
- `tests/test_telemetry.py` [MỚI]: 8 unit test kiểm tra ngưỡng định nghĩa timeout 500ms, validate magic và CRC, máy trạng thái mất liên kết/tự phục hồi, định dạng JSON WebSocket và kiểm tra tích hợp mã nguồn.
- `docs/pending_hardware_verification/task-07.md` [MỚI]: Quy trình kiểm chứng thực nghiệm tắt/bật nguồn RX kiểm tra cảnh báo < 1s, đánh dấu `[BLOCKED]` do phòng lab chưa cắm phần cứng.

## 3. Xác nhận phạm vi file
- Hoàn toàn KHÔNG đụng đến `rx/`, `common/protocol.h`, `knowledge.md`.
- Các file phụ trợ (`tests/*`, `docs/*`, `CMakeLists.txt`, `main.c`, `tx_server.*`, `tm1638.c`) chỉ phục vụ đăng ký build, boot, và hook phát WebSocket theo đúng quy chuẩn đã được plan phê duyệt và nêu trong `auditor.txt`.
