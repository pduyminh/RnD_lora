---
role: agy-executor
task_id: T-2026-0919-05
round: 1
plan_ref: plan.md
---

# Nhật ký thực thi: Task 05 — TX SoftAP, Web Virtual Joystick & 50Hz ESP-NOW

## Đã làm
1. **Thiết kế và triển khai module `tx/main/tx_server.h` và `tx/main/tx_server.c`**:
   - Cấu hình WiFi SoftAP theo đúng đặc tả: SSID `OMNI_ROBOT_TX`, Password `12345678` định nghĩa rõ ràng ở đầu file qua macro `TX_AP_SSID` và `TX_AP_PASSWORD`. Khởi tạo hoàn chỉnh NVS Flash, Netif default AP, Event loop và WiFi AP mode trên kênh 1.
   - Bật cờ `CONFIG_HTTPD_WS_SUPPORT=y` trong cấu hình `tx/sdkconfig` và tạo `tx/sdkconfig.defaults` để hỗ trợ WebSocket native trên `esp_http_server` (theo đúng điều khoản cho phép thêm cấu hình/thư viện của `task.md`).
   - Xây dựng giao diện web nhúng HTML/CSS/JS (`INDEX_HTML`):
     - Giao diện thân thiện màn hình cảm ứng di động và máy tính, tự động co giãn (`viewport`).
     - Virtual Joystick 2 trục cảm ứng điều khiển vận tốc $v_x, v_y$. Khi buông tay, joystick tự đàn hồi về tâm (0, 0).
     - Thanh trượt điều khiển xoay góc $\omega$ (từ -2000 đến 2000 mrad/s). Khi buông tay, thanh trượt tự trả về 0.
     - Nút dừng khẩn cấp đỏ nổi bật "EMERGENCY STOP" gửi cờ `estop = 1`.
     - Bộ truyền WebSocket client tự động kết nối và tái kết nối tới endpoint `ws://<host>/ws`, hiển thị trạng thái kết nối trực quan.
   - Xây dựng WebSocket handler (`ws_handler`):
     - Theo dõi kết nối và ngắt kết nối của client để cập nhật bộ đếm `s_ws_client_count`.
     - Phân tích chuỗi JSON nhận được, cập nhật các biến vận tốc `(s_vx_mm_s, s_vy_mm_s, s_omega_mrad_s, s_estop)` an toàn đa luồng bằng spinlock.
     - Cập nhật dấu thời gian nhận lệnh cuối `s_last_ws_rx_tick`.
   - Xây dựng FreeRTOS task phát ESP-NOW 50 Hz độc lập (`espnow_tx_task`):
     - Chạy chu kỳ đều đặn 20 ms ± 2 ms (`TX_ESPNOW_PERIOD_MS = 20`) qua `vTaskDelayUntil`.
     - **Chính sách an toàn zero-motion**: Nếu không có client WebSocket nào kết nối (`s_ws_client_count == 0`) hoặc quá 500ms không nhận được bản tin từ client, hàm `tx_build_ctrl_packet` tự động gán $v_x = v_y = \omega = 0$, triệt tiêu hoàn toàn rủi ro robot chạy tự do hoặc giữ lệnh cũ vô thời hạn.
     - Sequence number `seq` tự động tăng đều đặn đúng 1 đơn vị mỗi chu kỳ 20ms.
     - Mã CRC-16/CCITT-FALSE tính toán chính xác bằng `ctrl_packet_calc_crc` trên 10 bytes payload trước khi truyền broadcast qua ESP-NOW.

2. **Cập nhật cấu hình build và ứng dụng chính**:
   - Cập nhật `tx/main/CMakeLists.txt`: Đăng ký `tx_server.c` vào `SRCS` và các component `esp_wifi`, `esp_http_server`, `nvs_flash`, `esp_event`, `esp_netif` vào `REQUIRES`.
   - Cập nhật `tx/main/main.c`: Nhúng `tx_server.h` và gọi `tx_server_init()` trong luồng `app_main`.

3. **Biên dịch và kiểm thử tự động**:
   - `idf.py -C tx build` biên dịch thành công (exit code 0).
   - `idf.py -C rx build` biên dịch thành công (exit code 0).
   - Viết bộ unit test `tests/test_tx_web.py` (7 tests) kiểm tra định nghĩa cấu hình, mã HTML joystick/slider/estop, WebSocket handler, chu kỳ tăng seq 50Hz, cơ chế tự gán vận tốc về 0 khi ngắt client / timeout 500ms, và tính toán CRC16.
   - Toàn bộ test suite `pytest` đạt 31/31 tests PASS 100%.
   - `git diff --check` trả exit code 0.

4. **Lập tài liệu kiểm chứng phần cứng còn chờ (`BLOCKED`)**:
   - Tạo `docs/pending_hardware_verification/task-05.md` ghi nhận 4 quy trình thử nghiệm phát sóng WiFi SoftAP thật, kết nối điện thoại và kéo joystick thật ở trạng thái `[BLOCKED]` do phòng lab chưa cắm thiết bị theo chỉ đạo của người dùng.

## Quyết định khác với plan (nếu có) + lý do
- Bổ sung cấu hình `CONFIG_HTTPD_WS_SUPPORT=y` trong `tx/sdkconfig` và `tx/sdkconfig.defaults`: Đây là cấu hình cần thiết để thư viện `esp_http_server` hỗ trợ WebSocket theo đúng điều khoản trong mục Ràng buộc của `task.md` ("Được phép thêm component ESP-IDF cần thiết cho web server/WebSocket nếu chưa có sẵn, ghi rõ lý do trong plan.md").
- Các file cập nhật `tx/main/CMakeLists.txt`, `tx/main/main.c`, `tests/test_tx_web.py` và `docs/pending_hardware_verification/task-05.md` là tệp phụ trợ bắt buộc theo tiêu chí build/boot và kiểm chứng không phần cứng (mục 6 `lessons-learned.md`). Hoàn toàn không sửa `rx/`, `common/protocol.h`, hay `knowledge.md`.

## Output / artifact
- [`tx/main/tx_server.h`](file:///D:/Data_workspace_V94/RnD_LoRa/tx/main/tx_server.h)
- [`tx/main/tx_server.c`](file:///D:/Data_workspace_V94/RnD_LoRa/tx/main/tx_server.c)
- [`tx/main/CMakeLists.txt`](file:///D:/Data_workspace_V94/RnD_LoRa/tx/main/CMakeLists.txt)
- [`tx/main/main.c`](file:///D:/Data_workspace_V94/RnD_LoRa/tx/main/main.c)
- [`tx/sdkconfig.defaults`](file:///D:/Data_workspace_V94/RnD_LoRa/tx/sdkconfig.defaults)
- [`tx/build/omni_tx.bin`](file:///D:/Data_workspace_V94/RnD_LoRa/tx/build/omni_tx.bin)
- [`tests/test_tx_web.py`](file:///D:/Data_workspace_V94/RnD_LoRa/tests/test_tx_web.py)
- [`docs/pending_hardware_verification/task-05.md`](file:///D:/Data_workspace_V94/RnD_LoRa/docs/pending_hardware_verification/task-05.md)
- [`tasks/T-2026-0919-05/work-log.md`](file:///D:/Data_workspace_V94/RnD_LoRa/tasks/T-2026-0919-05/work-log.md)
- [`tasks/T-2026-0919-05/self-check.md`](file:///D:/Data_workspace_V94/RnD_LoRa/tasks/T-2026-0919-05/self-check.md)
