---
role: agy-executor
task_id: T-2026-0919-04
round: 1
plan_ref: plan.md
---

# Nhật ký thực thi: Task 04 — RX ESP-NOW Pipeline & Failsafe

## Đã làm
1. **Thiết kế và triển khai module `rx/main/comm.h` và `rx/main/comm.c`**:
   - Khởi tạo ngăn xếp WiFi Station và ESP-NOW của ESP-IDF v6.
   - Cài đặt bộ lọc xác thực gói tin nghiêm ngặt trong `comm_process_packet`:
     - Kiểm tra kích thước gói: 12 bytes (`sizeof(ctrl_packet_t)`).
     - Kiểm tra magic byte: `pkt->magic == CTRL_PACKET_MAGIC` (0xA5).
     - Kiểm tra toàn vẹn dữ liệu: tính CRC-16/CCITT-FALSE trên 10 bytes payload đầu và so khớp với `pkt->crc16`.
     - Kiểm tra tính tuần tự: so sánh `diff = (int16_t)(pkt->seq - s_last_seq) <= 0` để phát hiện và loại bỏ các gói tin cũ hoặc lặp lại (chống replay attack), đồng thời xử lý chính xác hiện tượng xoay vòng tràn số 16-bit (wraparound).
     - Bộ đếm `s_dropped_packet_count` tự động tăng và log UART cảnh báo mỗi khi phát hiện gói vi phạm bất kỳ tiêu chí nào.
   - Xử lý lệnh dừng khẩn cấp E-Stop:
     - Khi `pkt->estop == 1`, ngắt xung tức thì trên cả 3 trục qua `axis_stop_all()` ngay trong vòng lặp hiện tại, không đợi qua bộ ramp.
     - TUYỆT ĐỐI KHÔNG can thiệp tắt ENA, bảo toàn mô-men khóa giữ trục theo đúng chính sách mục 1.3.1 `knowledge.md`.
   - Cơ chế Failsafe giám sát đường truyền (chu kỳ 20ms / 50Hz):
     - FreeRTOS task `pipeline_task` chạy chu kỳ 20ms thực hiện `comm_pipeline_step`.
     - Nếu quá 300 ms liên tục không nhận được gói tin hợp lệ: kích hoạt failsafe, log rõ dòng ký tự `FAILSAFE TRIGGERED` ra UART, đặt target PPS về 0 và thực hiện ramp giảm tốc mượt mà (Anti-Jerk S-curve) về 0 bằng `kinematics_ramp_update`, đảm bảo robot dừng êm ái mà không bị giật khựng cơ khí.
     - Giữ nguyên chân ENA ở trạng thái enable (mức LOW active) trong suốt và sau khi failsafe diễn ra.
   - Phản hồi Telemetry về TX:
     - Sau mỗi gói tin hợp lệ, tự động kiểm tra và đăng ký peer ESP-NOW từ địa chỉ MAC nguồn (`s_last_sender_mac`).
     - Đóng gói `telemetry_packet_t` (8 bytes packed) chứa `magic = TELEMETRY_PACKET_MAGIC` (0x5A), `seq_echo = pkt->seq`, `link_ok = 1`, `driver_fault_bitmap = 0`, `reserved = 0x00`, tính CRC-16 và truyền qua ESP-NOW unicast về TX.

2. **Cập nhật cấu hình build và ứng dụng chính**:
   - Cập nhật `rx/main/CMakeLists.txt`: Thêm `comm.c` vào `SRCS` và các component `esp_wifi`, `nvs_flash`, `esp_event`, `esp_netif` vào `REQUIRES`.
   - Cập nhật `rx/main/main.c`: Tích hợp `#include "comm.h"`, gọi `comm_init()` trong luồng `app_main`, và kiểm thử self-test tiêm gói tin giả lập.

3. **Biên dịch và kiểm thử tự động**:
   - `idf.py -C rx build` biên dịch thành công (exit code 0), nhị phân sinh ra đầy đủ tại `rx/build/omni_rx.bin` (kích thước 0xbe7c0 bytes).
   - `idf.py -C tx build` biên dịch thành công (exit code 0).
   - Viết bộ unit test `tests/test_rx_pipeline.py` (7 tests) kiểm tra toàn diện: kiểm tra file tồn tại, kiểm tra loại bỏ sai magic/CRC, chống lặp gói và xoay vòng seq, E-stop cắt xung tức thì duy trì ENA, timeout 300ms kích hoạt failsafe với ramp giảm tốc êm ái duy trì ENA, và cấu trúc gói tin telemetry phản hồi.
   - Toàn bộ test suite `pytest` đạt 24/24 tests PASS 100%.
   - `git diff --check` trả exit code 0.

4. **Lập tài liệu kiểm chứng phần cứng còn chờ (`BLOCKED`)**:
   - Tạo `docs/pending_hardware_verification/task-04.md` mô tả chi tiết 4 quy trình thử nghiệm ghép nối thực tế giữa 2 thiết bị ESP32 (TX và RX) ở trạng thái `[BLOCKED]` do phòng lab chưa kết nối phần cứng theo chỉ đạo của người dùng.

## Quyết định khác với plan (nếu có) + lý do
- Bổ sung đăng ký trong `rx/main/CMakeLists.txt`, tích hợp gọi `comm_init()` trong `rx/main/main.c`, tạo unit test `tests/test_rx_pipeline.py` và tài liệu `docs/pending_hardware_verification/task-04.md`: Đây là các tệp phụ trợ bắt buộc theo tiêu chí chấp nhận số 1 (`idf.py build` thành công cho `rx/`), tiêu chí số 2/3/4/5 (kiểm tra hoạt động pipeline khi boot), và quy tắc kiểm chứng không phần cứng (mục 1 và mục 6 `lessons-learned.md`). Không sửa đổi logic các file đã hoàn thành ở task trước (`rx/main/kinematics.c`, `rx/main/axis_driver.c`, `common/protocol.h`, `tx/`, `knowledge.md`).

## Output / artifact
- [`rx/main/comm.h`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/comm.h)
- [`rx/main/comm.c`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/comm.c)
- [`rx/main/CMakeLists.txt`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/CMakeLists.txt)
- [`rx/main/main.c`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/main.c)
- [`rx/build/omni_rx.bin`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/build/omni_rx.bin)
- [`tests/test_rx_pipeline.py`](file:///D:/Data_workspace_V94/RnD_LoRa/tests/test_rx_pipeline.py)
- [`docs/pending_hardware_verification/task-04.md`](file:///D:/Data_workspace_V94/RnD_LoRa/docs/pending_hardware_verification/task-04.md)
- [`tasks/T-2026-0919-04/work-log.md`](file:///D:/Data_workspace_V94/RnD_LoRa/tasks/T-2026-0919-04/work-log.md)
- [`tasks/T-2026-0919-04/self-check.md`](file:///D:/Data_workspace_V94/RnD_LoRa/tasks/T-2026-0919-04/self-check.md)
