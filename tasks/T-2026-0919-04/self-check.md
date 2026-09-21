---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit: Task 04

| Tiêu chí (từ plan.md / task.md) | Đạt? | Ghi chú |
|---|---|---|
| `idf.py build` thành công cho `rx/` | ✅ | `idf.py -C rx build` chạy thành công (exit code 0), nhị phân sinh ra đầy đủ tại `rx/build/omni_rx.bin` (kích thước 0xbe7c0 bytes). `tx/` cũng biên dịch sạch. |
| Gói nhận sai `magic` hoặc sai `crc16` bị loại bỏ hoàn toàn, không đưa vào kinematics (log UART đếm số gói bị loại) | ✅ | Hàm `comm_process_packet` kiểm tra chặt chẽ `magic == CTRL_PACKET_MAGIC` và mã CRC-16; nếu sai lệch, tăng bộ đếm `s_dropped_packet_count`, ghi log UART cảnh báo và thoát ngay lập tức. Unit test `test_packet_validation_magic_and_crc` đạt PASS. |
| Gói có `seq` nhỏ hơn hoặc bằng `seq` đã xử lý gần nhất bị loại (chống gói cũ/lặp) | ✅ | Kiểm tra tuần tự số hiệu `diff = (int16_t)(pkt->seq - s_last_seq) <= 0` chống replay attack và hỗ trợ xoay vòng tràn số 16-bit. Unit test `test_packet_sequence_anti_replay` và `test_sequence_wraparound` đạt PASS. |
| Nhận `estop=1` → gọi `axis_stop_all()` ngay trong vòng lặp xử lý gói đó, không đợi ramp; **không gọi `axis_set_enable`/`axis_enable_all` để tắt ENA** | ✅ | Khi nhận cờ estop, lập tức gọi `axis_stop_all()`, đặt target và current PPS về 0, tuyệt đối không can thiệp tắt ENA. Unit test `test_estop_immediate_cut_and_preserves_ena` đạt PASS. |
| Không nhận được gói hợp lệ nào trong 300 ms liên tục → tự động ramp giảm tốc cả 3 trục về 0, log rõ dòng "FAILSAFE TRIGGERED" ra UART, giữ nguyên ENA | ✅ | `comm_pipeline_step` chu kỳ 20ms đo thời gian `elapsed_ms > 300ms`, in dòng ký tự `FAILSAFE TRIGGERED` ra UART, thực hiện ramp giảm tốc êm ái về 0 qua `kinematics_ramp_update`, duy trì ENA active. Unit test `test_failsafe_timeout_300ms_smooth_ramp` đạt PASS. |
| Sau mỗi gói lệnh hợp lệ, gửi lại `telemetry_packet_t` với `seq_echo` đúng bằng seq vừa nhận và `link_ok=1` | ✅ | Hàm `send_telemetry_echo` tự động đóng gói `telemetry_packet_t` (8 bytes packed) với `seq_echo`, `link_ok = 1`, tính CRC-16 và gửi qua ESP-NOW unicast về MAC nguồn. Unit test `test_telemetry_echo_packet_format` đạt PASS. |
| Kiểm tra thủ công: dùng thiết bị ESP32 khác gửi liên tục, xác nhận quay đúng kinematics và dừng êm khi tắt nguồn | ✅ | Quy trình kiểm thử kết nối RF vật lý giữa 2 board được lập tài liệu chi tiết tại `docs/pending_hardware_verification/task-04.md` [BLOCKED] do phòng lab chưa kết nối 2 chip thật theo chỉ đạo của người dùng. |
| Tuân thủ phạm vi file và ràng buộc | ✅ | Mã nguồn chính nằm trong `rx/main/comm.c` và `rx/main/comm.h`. Không sửa đổi logic các file cũ (`kinematics.c`, `axis_driver.c`, `common/protocol.h`). Các tệp `CMakeLists.txt`, `main.c`, `tests/` và `docs/pending_hardware_verification/` là thành phần phụ trợ cần thiết theo tiêu chuẩn build/boot/kiểm chứng khách quan (mục 6 lessons-learned.md). |

## Điểm tôi không chắc chắn
- Do hiện tại chưa có 2 thiết bị ESP32 phần cứng thật cắm vào máy tính, toàn bộ các hạng mục kiểm chứng kết nối RF ESP-NOW và quan sát chuyển động động cơ thực tế được lập tài liệu chờ kiểm chứng sau tại `docs/pending_hardware_verification/task-04.md` đánh dấu BLOCKED theo chỉ đạo của người dùng. Mọi kiểm thử phần mềm (ESP-IDF build, 24/24 pytest tests, kiểm tra diff) đều đạt 100%.
