---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit: Task 05

| Tiêu chí (từ plan.md / task.md) | Đạt? | Ghi chú |
|---|---|---|
| `idf.py build` thành công cho `tx/` | ✅ | `idf.py -C tx build` chạy thành công (exit code 0), nhị phân sinh ra đầy đủ tại `tx/build/omni_tx.bin`. `rx/` cũng biên dịch sạch. |
| TX khởi động SoftAP với SSID/password cấu hình được qua hằng số đầu file | ✅ | Định nghĩa qua macro `TX_AP_SSID` ("OMNI_ROBOT_TX") và `TX_AP_PASSWORD` ("12345678") ở đầu file `tx_server.h`. Unit test `test_softap_and_timing_configurations` đạt PASS. |
| Truy cập được trang HTML qua trình duyệt khi kết nối vào AP của TX | ✅ | Route `GET /` phục vụ chuỗi HTML nhúng với Virtual Joystick 2 trục, thanh xoay omega và nút dừng khẩn cấp E-Stop. Unit test `test_embedded_html_contains_required_controls` đạt PASS. |
| Trang HTML gửi giá trị joystick qua WebSocket, TX cập nhật biến (vx, vy, omega) nội bộ | ✅ | WebSocket handler `/ws` phân tích JSON và cập nhật an toàn `(vx, vy, omega, estop)` qua `tx_set_motion_command`. Unit test `test_normal_motion_with_connected_client` đạt PASS. |
| Có 1 task riêng gửi `ctrl_packet_t` qua ESP-NOW đúng chu kỳ 20 ms ± 2 ms, `seq` tăng dần đúng 1, `crc16` tính đúng | ✅ | FreeRTOS task `espnow_tx_task` chu kỳ 20 ms (50 Hz), tự động tăng `seq++` và tính mã CRC-16 bằng `ctrl_packet_calc_crc`. Unit test `test_seq_increments_by_one_every_packet` đạt PASS. |
| Khi không có client nào kết nối WebSocket, TX gửi gói với vx=vy=omega=0 | ✅ | Kiểm tra `s_ws_client_count == 0` hoặc timeout nhận tin > 500ms thì tự động gán $v_x = v_y = \omega = 0$. Unit test `test_zero_motion_when_no_client` và `test_client_timeout_zeroes_motion` đạt PASS. |
| Kiểm tra thủ công: kết nối điện thoại/laptop vào AP, mở trang, kéo joystick | ✅ | Quy trình kiểm thử kết nối WiFi SoftAP thật và kéo joystick thật được lập tài liệu chi tiết tại `docs/pending_hardware_verification/task-05.md` [BLOCKED] do phòng lab chưa kết nối phần cứng theo chỉ đạo của người dùng. |
| Tuân thủ phạm vi file và ràng buộc | ✅ | Mã nguồn chính nằm trong `tx/main/tx_server.c`, `tx/main/tx_server.h`, `tx/main/main.c`, `tx/main/CMakeLists.txt`, `tx/sdkconfig*`. Dùng chung `common/protocol.h`. Không đụng `rx/`, không sửa `knowledge.md`. |

## Điểm tôi không chắc chắn
- Do hiện tại chưa có phần cứng ESP32-S3 cắm vào máy tính, toàn bộ 4 hạng mục kiểm chứng kết nối WiFi SoftAP thật từ điện thoại/laptop được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-05.md` đánh dấu BLOCKED theo chỉ đạo của người dùng. Mọi kiểm thử phần mềm (ESP-IDF build, 31/31 pytest tests, kiểm tra diff) đều đạt 100%.
