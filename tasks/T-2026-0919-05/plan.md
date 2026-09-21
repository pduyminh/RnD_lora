---
role: codex-advisor
task_id: T-2026-0919-05
round: 1
---

# Khuyến nghị thực thi: Task 05 — TX SoftAP, Web Virtual Joystick & 50Hz ESP-NOW

## Cách tiếp cận đề xuất
1. Cài đặt module mạng và Web Server trong `tx/main/tx_server.h` và `tx/main/tx_server.c`.
2. Cấu hình WiFi SoftAP:
   - SSID: `OMNI_ROBOT_TX`, Password: `12345678` (định nghĩa rõ ràng qua macro đầu file `TX_AP_SSID`, `TX_AP_PASSWORD`).
   - Khởi tạo ngăn xếp mạng: NVS Flash, Netif default AP, Event loop, WiFi AP mode channel 1.
3. Cài đặt HTTP Web Server (`esp_http_server`):
   - Kích hoạt cấu hình WebSocket trong ESP-IDF qua `CONFIG_HTTPD_WS_SUPPORT=y` (ghi nhận trong `tx/sdkconfig` và `tx/sdkconfig.defaults` theo đúng điều khoản được phép thêm cấu hình/component cần thiết trong `task.md`).
   - Endpoint `GET /`: Phục vụ giao diện HTML/CSS/JS nhúng gồm Virtual Joystick 2 trục ($v_x, v_y$), thanh trượt xoay ($\omega$), và nút dừng khẩn cấp đỏ "EMERGENCY STOP".
   - Khi buông tay khỏi joystick: vị trí tự động trở về tâm và gửi lệnh $v_x = 0, v_y = 0$.
   - Endpoint `GET /ws`: WebSocket handler nhận bản tin JSON `{"vx":..., "vy":..., "omega":..., "estop":...}` và cập nhật trạng thái lệnh chuyển động (thread-safe).
4. Cài đặt FreeRTOS task phát ESP-NOW 50 Hz độc lập (`TX_ESPNOW_PERIOD_MS = 20`):
   - Chạy chu kỳ đều đặn 20 ms ± 2 ms bất kể có tin nhắn mới từ WebSocket hay không, đảm bảo bộ đếm failsafe 300ms của RX hoạt động ổn định.
   - Mỗi chu kỳ tăng `seq` đúng 1 đơn vị.
   - **Chính sách an toàn khi không có client**: Nếu không có client WebSocket kết nối (`s_ws_client_count == 0`) hoặc quá 500ms không nhận được gói từ client, TX tự động gán $v_x = v_y = \omega = 0$ (chỉ duy trì cờ estop nếu có), không bao giờ gửi giá trị rác hay giữ nguyên lệnh cuối vô thời hạn.
   - Tính toán mã CRC-16/CCITT-FALSE chuẩn xác bằng `ctrl_packet_calc_crc` và gửi broadcast qua ESP-NOW.
5. Tích hợp và kiểm chứng:
   - Cập nhật `tx/main/CMakeLists.txt` bổ sung `tx_server.c` và các component phụ trợ: `esp_wifi`, `esp_http_server`, `nvs_flash`, `esp_event`, `esp_netif`.
   - Cập nhật `tx/main/main.c` gọi `tx_server_init()` trong luồng `app_main`.
   - Viết bộ unit test `tests/test_tx_web.py` (7 tests) kiểm tra cấu hình hằng số, phần tử HTML/JS, logic gán vận tốc về 0 khi không có client, chu kỳ tăng seq, và tính toán CRC16.
   - Lập tài liệu kiểm chứng phần cứng còn chờ `docs/pending_hardware_verification/task-05.md` [BLOCKED].

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `tx/main/tx_server.h`, `tx/main/tx_server.c`, `tx/main/CMakeLists.txt`, `tx/main/main.c`, `tx/sdkconfig`, `tx/sdkconfig.defaults`, `tests/test_tx_web.py`, `docs/pending_hardware_verification/task-05.md`, các file nhật ký trong `tasks/T-2026-0919-05/`.
- CẤM sửa `rx/`, `common/protocol.h`, `knowledge.md`, `task.md`.
- Dùng chung struct `ctrl_packet_t` từ `common/protocol.h`, không tự ý định nghĩa lại struct.
- Các tiêu chí kiểm chứng kết nối WiFi/Web từ điện thoại thật được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-05.md` đánh dấu `[BLOCKED]`.

## Rủi ro cần lưu ý
- Cấu hình WebSocket trong ESP-IDF: Cần cờ `CONFIG_HTTPD_WS_SUPPORT=y` để header `esp_http_server.h` khai báo cấu trúc `httpd_ws_frame_t` và trường `is_websocket`.
- Broadcast ESP-NOW: Cần thêm peer broadcast `FF:FF:FF:FF:FF:FF` vào danh sách peer trước khi gọi `esp_now_send`.

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- `idf.py -C tx build` trả exit code 0.
- `idf.py -C rx build` trả exit code 0.
- `python -m pytest tests -v --tb=short` pass 100% (31/31 tests).
- Có đầy đủ chức năng SoftAP, Web Virtual Joystick, WebSocket handler, 50Hz sender và an toàn zero-motion khi mất client.
- Có tài liệu kiểm chứng phần cứng [BLOCKED] cho task-05.
