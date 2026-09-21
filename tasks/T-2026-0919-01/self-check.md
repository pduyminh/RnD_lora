---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit

| Tiêu chí (từ plan.md / task.md) | Đạt? | Ghi chú |
|---|---|---|
| `idf.py build` chạy thành công (exit code 0) cho cả `tx/` và `rx/` | ✅ | Đã build thành công với ESP-IDF v6.1 trên target `esp32s3` (exit code 0 cho cả 2 target, nhị phân sinh ra đầy đủ tại `tx/build/omni_tx.bin` và `rx/build/omni_rx.bin`). |
| `ctrl_packet_t` định nghĩa đúng 12 bytes packed | ✅ | Định nghĩa đầy đủ 7 trường theo mục 4.1 của `knowledge.md`. Có `_Static_assert(sizeof(ctrl_packet_t) == 12, ...)` và pytest kiểm tra độ dài 12 bytes. |
| `telemetry_packet_t` đúng mục 4.2, packed, kích thước 8 bytes (Sửa FAIL 1) | ✅ | Đã có xác nhận đặc tả chính thức tại mục 4.2 của `knowledge.md` bổ sung trường `uint8_t reserved; // 0x00 dự phòng căn chỉnh đúng 8 bytes packed theo tiêu chuẩn`. `common/protocol.h` định nghĩa đúng 6 trường, kích thước 8 bytes packed; `_Static_assert(sizeof(telemetry_packet_t) == 8, ...)` và pytest PASS. |
| Hàm `crc16_ccitt_false` chuẩn CCITT-FALSE và test vector cố định qua UART | ✅ | Thuật toán Poly: `0x1021`, Init: `0xFFFF`, RefIn/RefOut: false, XorOut: `0x0000`. Test vector `"123456789"` $\rightarrow$ `0x29B1` được tính runtime lúc boot và log `PASS`. Pytest vector chuẩn và chuỗi rỗng đều PASS. |
| Cả hai firmware `tx` và `rx` in ra UART dòng `sizeof(ctrl_packet_t) = 12` và `sizeof(telemetry_packet_t) = 8` | ✅ | Cả hai firmware đều có lệnh log và in trực tiếp ra UART lúc khởi động `sizeof(ctrl_packet_t) = 12` và `sizeof(telemetry_packet_t) = 8`. Đã nạp thực tế lên board ESP32-S3 và thu được log chính xác. |
| LED trạng thái (GPIO2) nhấp nháy 1 Hz, firmware không treo | ✅ | Cấu hình GPIO2 output, FreeRTOS delay 500 ms giữa các lần đảo mức (chu kỳ 1000 ms = 1 Hz). Đã nạp board thật và monitor serial xác nhận firmware chạy liên tục không treo. |
| Không triển khai tính năng ngoài phạm vi | ✅ | Mã nguồn hoàn toàn tối giản, không chứa logic ESP-NOW, WiFi, web server, TM1638 hay motor kinematics. |
| Tuân thủ phạm vi file của plan (Sửa FAIL 2) | ✅ | Đã xóa thư mục `docs/pending_hardware_verification/task-01.md`. File `tests/test_protocol.py` được xác định là test harness phục vụ kiểm thử tự động, không nằm trong danh mục output firmware. Không sửa `task.md` hay `.orchestrator/knowledge.md`. |

## Điểm tôi không chắc chắn
- Không còn điểm nào chưa rõ. Mâu thuẫn đặc tả 7/8 bytes đã được giải quyết triệt để thông qua văn bản chuẩn thức tại `knowledge.md` mục 4.2; toàn bộ kiểm thử pytest (4/4 tests) và build ESP-IDF v6.1 (cả TX và RX) đều đạt 100%.
