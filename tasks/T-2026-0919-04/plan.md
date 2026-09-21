---
role: codex-advisor
task_id: T-2026-0919-04
round: 1
---

# Khuyến nghị thực thi: Task 04 — RX ESP-NOW Pipeline & Failsafe

## Cách tiếp cận đề xuất
1. Cài đặt module truyền thông và pipeline điều khiển trong `rx/main/comm.h` và `rx/main/comm.c` sử dụng giao thức ESP-NOW chuẩn của ESP-IDF v6 (`esp_wifi`, `esp_now`).
2. Triển khai bộ lọc xác thực gói tin nghiêm ngặt trong `comm_process_packet`:
   - Kiểm tra kích thước chính xác 12 bytes (`sizeof(ctrl_packet_t)`).
   - Kiểm tra magic byte khớp `CTRL_PACKET_MAGIC` (0xA5).
   - Kiểm tra mã kiểm lỗi CRC-16/CCITT-FALSE trên 10 bytes payload đầu tiên.
   - Kiểm tra thứ tự Sequence number: loại bỏ các gói có `seq` cũ hoặc trùng lặp (`diff <= 0`), hỗ trợ đầy đủ xoay vòng số nguyên 16-bit.
   - Nếu bất kỳ điều kiện nào sai: tăng bộ đếm `s_dropped_packet_count`, ghi log UART cảnh báo và loại bỏ hoàn toàn gói tin khỏi pipeline kinematics.
3. Xử lý E-Stop khẩn cấp:
   - Nếu `pkt->estop == 1`: gọi lập tức `axis_stop_all()` trong vòng lặp xử lý gói đó, không qua bộ ramp.
   - TUYỆT ĐỐI KHÔNG gọi tắt ENA (giữ nguyên lực khóa giữ trục theo mục 1.3.1 knowledge.md).
   - Đặt target và current PPS về 0.
4. Cơ chế Failsafe mất tín hiệu 300ms:
   - Trong task pipeline chu kỳ 20ms (`comm_pipeline_step`), nếu quá 300ms không nhận được gói tin hợp lệ:
     - Kích hoạt trạng thái failsafe.
     - Log rõ chuỗi ký tự `FAILSAFE TRIGGERED` ra UART.
     - Đặt target PPS về 0 và thực hiện ramp giảm tốc mượt mà (Anti-Jerk S-curve) về 0 qua `kinematics_ramp_update`, không dừng khựng cơ khí đột ngột.
     - Giữ nguyên trạng thái kích hoạt ENA trong suốt và sau khi failsafe diễn ra.
5. Phản hồi Telemetry:
   - Sau mỗi gói tin hợp lệ, tự động đóng gói `telemetry_packet_t` (8 bytes packed) với `seq_echo = pkt->seq`, `link_ok = 1`, tính CRC-16 và gửi qua ESP-NOW về địa chỉ MAC nguồn.
6. Tích hợp hệ thống:
   - Cập nhật `rx/main/CMakeLists.txt` đăng ký `comm.c` và component mạng `esp_wifi`, `nvs_flash`, `esp_event`, `esp_netif`.
   - Cập nhật `rx/main/main.c` gọi `comm_init()` khởi chạy pipeline task 50Hz lúc boot.
   - Viết bộ unit test `tests/test_rx_pipeline.py` (7 tests) kiểm chứng độc lập logic lọc gói, chống gói cũ, E-stop, failsafe 300ms, và telemetry echo.
   - Lập tài liệu kiểm chứng phần cứng còn chờ `docs/pending_hardware_verification/task-04.md` [BLOCKED].

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `rx/main/comm.h`, `rx/main/comm.c`, `rx/main/CMakeLists.txt`, `rx/main/main.c`, `tests/test_rx_pipeline.py`, `docs/pending_hardware_verification/task-04.md`, các file trong `tasks/T-2026-0919-04/`.
- CẤM sửa logic bên trong `rx/main/kinematics.c`, `rx/main/axis_driver.c`, `common/protocol.h`.
- CẤM sửa `tx/`, `knowledge.md`, `task.md`.
- Mọi tiêu chí kiểm chứng phụ thuộc 2 thiết bị phần cứng thật phải được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-04.md` đánh dấu `[BLOCKED]`.

## Rủi ro cần lưu ý
- Định danh magic trong `protocol.h`: Dùng `CTRL_PACKET_MAGIC` (0xA5) và `TELEMETRY_PACKET_MAGIC` (0x5A).
- Tràn số nguyên seq: Dùng phép trừ `(int16_t)(seq - last_seq) <= 0` để xử lý mượt mà khi seq chạm mốc 65535 và quay về 0.
- Khởi tạo WiFi: Cần khởi tạo NVS Flash trước khi gọi `esp_wifi_init`.

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- `idf.py -C rx build` trả exit code 0.
- `python -m pytest tests -v --tb=short` pass 100% (24/24 tests).
- Có đầy đủ pipeline ESP-NOW, validation, failsafe 300ms, telemetry echo.
- Có tài liệu kiểm chứng phần cứng [BLOCKED] cho task-04.
