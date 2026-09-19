---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit

| Tiêu chí (từ plan.md / task.md) | Đạt? | Ghi chú |
|---|---|---|
| `idf.py build` chạy thành công (exit code 0) cho cả `tx/` và `rx/` | ✅ | Đã build thành công với ESP-IDF v6.1 trên target `esp32s3` (exit code 0 cho cả 2 target, nhị phân sinh ra đầy đủ tại `tx/build/omni_tx.bin` và `rx/build/omni_rx.bin`). |
| `common/protocol.h` định nghĩa đúng `ctrl_packet_t` (12 bytes) và `telemetry_packet_t` (8 bytes) packed | ✅ | `ctrl_packet_t` 12 bytes; `telemetry_packet_t` 8 bytes (bổ sung byte `reserved` trước `crc16`); có `_Static_assert` compile-time kiểm tra chặt chẽ cả hai struct và payload trước CRC. |
| Hàm `crc16_ccitt_false` chuẩn CCITT-FALSE và test vector cố định qua UART | ✅ | Thuật toán Poly: `0x1021`, Init: `0xFFFF`, RefIn/RefOut: false, XorOut: `0x0000`. Test vector `"123456789"` $\rightarrow$ `0x29B1` được tính runtime lúc boot và log `PASS`. |
| Cả hai firmware `tx` và `rx` in ra UART dòng `sizeof(ctrl_packet_t) = 12` và `sizeof(telemetry_packet_t) = 8` | ✅ | Đã nạp thực tế lên board ESP32-S3 (COM10, MAC `68:ee:8f:4d:7d:e4`) cho cả hai firmware. Log UART thực tế xác nhận in chính xác `sizeof(ctrl_packet_t) = 12` và `sizeof(telemetry_packet_t) = 8`. |
| LED trạng thái (GPIO2) nhấp nháy 1 Hz, firmware không treo | ✅ | Cấu hình GPIO2 output, FreeRTOS delay 500 ms giữa các lần đảo mức (chu kỳ 1000 ms = 1 Hz). Đã nạp board thật và monitor serial xác nhận firmware chạy liên tục không reset loop. |
| Không triển khai tính năng ngoài phạm vi (ESP-NOW, WiFi, web server, TM1638, motor) | ✅ | Mã nguồn hoàn toàn tối giản, chỉ bao gồm header giao thức, log thông số cấu trúc, self-test CRC và task nhấp nháy GPIO2. |
| Không sửa file bị cấm | ✅ | Không sửa `task.md`, không sửa `.orchestrator/knowledge.md`, không sửa bất kỳ file nào trong `.orchestrator/`. |

## Điểm tôi không chắc chắn
- Hiện tại máy trạm chỉ kết nối 1 board ESP32-S3 tại cổng COM10; đã nạp lần lượt và kiểm chứng thành công cả 2 firmware `tx` và `rx` trên board này. Khi người dùng cấp thêm board thứ hai trên cổng COM khác, chỉ cần cắm vào là có thể nạp đồng thời cả 2 board.
