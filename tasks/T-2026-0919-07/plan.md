---
role: codex-advisor
task_id: T-2026-0919-07
round: 1
---

# Khuyến nghị thực thi: Task 07 — TX Telemetry Reception & Dual-Display Alert

## Cách tiếp cận đề xuất
1. Cài đặt module giám sát Telemetry trong `tx/main/telemetry.h` và `tx/main/telemetry.c`:
   - Định nghĩa ngưỡng timeout `TELEMETRY_TIMEOUT_MS = 500` ms (lớn hơn ngưỡng failsafe 300ms của RX để loại trừ báo động giả do độ trễ truyền thông).
   - Đăng ký hàm callback nhận ESP-NOW `espnow_telemetry_recv_cb` để đón gói tin `telemetry_packet_t` (8 bytes) từ RX gửi về.
   - Hàm `telemetry_process_packet()` kiểm tra tính hợp lệ:
     - `magic == TELEMETRY_PACKET_MAGIC` (0x5A)
     - `crc16 == telemetry_packet_calc_crc(pkt)`
     - Loại bỏ gói sai và tăng bộ đếm lỗi.
   - Khi nhận gói hợp lệ: ghi nhận `s_last_telemetry_tick = xTaskGetTickCount()`, lưu `seq_echo`, chuyển trạng thái sang `link_ok = true`.
2. Giám sát mất kết nối và tự phục hồi:
   - Background FreeRTOS task `telemetry_task` chạy chu kỳ 50ms kiểm tra quá 500ms không nhận được telemetry thì chuyển sang `link_ok = false`.
   - Khi mất kết nối:
     - Bật LED đơn thứ 2 trên module TM1638 (`tm1638_set_led(1, true)`).
     - Đẩy ngay thông báo WebSocket `{"rx_link":"MẤT KẾT NỐI","link_ok":false}` tới toàn bộ trình duyệt đang kết nối.
   - Khi có kết nối lại:
     - Tắt LED đơn thứ 2 trên module TM1638 (`tm1638_set_led(1, false)`).
     - Đẩy thông báo WebSocket `{"rx_link":"OK","link_ok":true}` tới toàn bộ trình duyệt.
     - Tự động phục hồi mà không cần khởi động lại board TX.
3. Tích hợp giao diện Web và TM1638:
   - Trong `tx/main/tx_server.h` và `tx/main/tx_server.c`: Bổ sung hàm phát thanh `tx_broadcast_ws_message()` gửi bản tin chuỗi JSON tới tất cả client WebSocket qua `httpd_ws_send_frame_async()`.
   - Trong `INDEX_HTML`: Bổ sung phần tử hiển thị `<div id='rx-status'>` và callback `ws.onmessage` tự động cập nhật màu sắc/nội dung thời gian thực không cần tải lại trang.
   - Trong `tx/main/tm1638.c`: Bổ sung lời gọi kiểm tra `telemetry_is_link_ok()` để cập nhật LED 2 (index 1).
   - Trong `tx/main/main.c`: Khởi tạo `telemetry_init()` sau khi đã khởi tạo xong TX server và TM1638.
   - Trong `tx/main/CMakeLists.txt`: Đăng ký `telemetry.c` vào `SRCS`.
4. Kiểm chứng:
   - Bộ unit test `tests/test_telemetry.py` (8 test) kiểm tra ngưỡng timeout, xác thực gói tin, máy trạng thái timeout/phục hồi, định dạng WebSocket JSON, phần tử HTML và tích hợp mã nguồn.
   - Lập tài liệu kiểm chứng phần cứng còn chờ `docs/pending_hardware_verification/task-07.md` [BLOCKED].

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `tx/main/telemetry.h`, `tx/main/telemetry.c`, `tx/main/tx_server.h`, `tx/main/tx_server.c` (chỉ thêm hook broadcast và giao diện rx-status), `tx/main/tm1638.c` (chỉ thêm cập nhật LED 2), `tx/main/CMakeLists.txt`, `tx/main/main.c`, `tests/test_telemetry.py`, `docs/pending_hardware_verification/task-07.md`, các file nhật ký trong `tasks/T-2026-0919-07/`.
- CẤM sửa `rx/`, `common/protocol.h`, `knowledge.md`, `task.md`.
- Sử dụng struct `telemetry_packet_t` chuẩn từ `common/protocol.h`.
- Tiêu chí rút điện RX thật được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-07.md` đánh dấu `[BLOCKED]`.

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- `idf.py -C tx build` trả exit code 0.
- `idf.py -C rx build` trả exit code 0.
- `python -m pytest tests -v --tb=short` pass 100% (47/47 tests).
- Nhận và xác thực `telemetry_packet_t`, timeout 500ms điều khiển LED 2 TM1638 và đẩy cập nhật qua WebSocket.
- Có tài liệu kiểm chứng phần cứng [BLOCKED] cho task-07.
