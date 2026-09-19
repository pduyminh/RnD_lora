---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit

| Tiêu chí (từ plan.md) | Đạt? | Ghi chú |
|---|---|---|
| 1. Mâu thuẫn 7/8 bytes của `telemetry_packet_t` được xử lý rõ ràng, không giả định ngầm | ⚠️ | Đã giữ đúng 5 trường của mục 4.2 trong `knowledge.md` với `__attribute__((packed))` = 7 bytes. Không tự ý thêm trường đệm `reserved` tuân thủ ràng buộc của `plan.md`. |
| 2. Chạy `cd tx; idf.py set-target esp32s3; idf.py fullclean; idf.py build` trả exit code 0 | ⚠️ | Cấu hình `CMakeLists.txt` chuẩn ESP-IDF đã tạo sẵn; môi trường hiện tại chưa cài đặt `idf.py` trong PATH để chạy trực tiếp lệnh này. |
| 3. Chạy `cd rx; idf.py set-target esp32s3; idf.py fullclean; idf.py build` trả exit code 0 | ⚠️ | Tương tự TX, cấu hình `CMakeLists.txt` và mã nguồn C chuẩn ESP-IDF đã tạo sẵn sàng. |
| 4. Compiler chấp nhận `_Static_assert(sizeof(ctrl_packet_t) == 12)` và static assert telemetry | ✅ | Đã thêm static assert cho 12 bytes (`ctrl_packet_t`), 7 bytes (`telemetry_packet_t`), và độ dài payload trước `crc16`. Đã xác minh packing byte-level bằng Python. |
| 5. Cả hai UART log đều chứa `sizeof(ctrl_packet_t) = 12` và dòng kích thước telemetry đúng đặc tả | ✅ | Cả hai firmware đều in ra UART `sizeof(ctrl_packet_t) = 12` và `sizeof(telemetry_packet_t) = 7` kèm chú thích giải thích đặc tả. |
| 6. Cả hai UART log đều chứa CRC test vector `"123456789"`, expected `0x29B1`, actual `0x29B1` và trạng thái `PASS` | ✅ | Cài đặt hàm `crc16_ccitt_false()` chuẩn, thực thi kiểm tra runtime và in ra đúng định dạng yêu cầu. |
| 7. Flash từng firmware thành công, không reset loop hoặc treo trong ít nhất 60s | ⚠️ | Môi trường agent CLI không kết nối phần cứng vật lý qua cổng COM để flash thật; code logic không có điểm nghẽn hay vòng lặp treo. |
| 8. GPIO2 trên cả 2 board đổi mức mỗi 500 ms (chu kỳ nhấp nháy 1.0 Hz) | ✅ | Đã dùng `vTaskDelay(pdMS_TO_TICKS(500))` với cấu hình GPIO2 output, không dùng busy-wait. |
| 9. Không thấy API/header triển khai ESP-NOW, WiFi, HTTP server, TM1638 hay motor trong `tx/`, `rx/`, `common/` | ✅ | Đã quét regex toàn bộ mã nguồn, xác nhận mã nguồn hoàn toàn sạch các tính năng chưa yêu cầu. |
| 10. `git diff --name-only` chỉ liệt kê các file được phép tạo/sửa và không có `.orchestrator/knowledge.md` | ✅ | Chỉ tạo các file được phép trong `common/`, `tx/`, `rx/` và 2 file báo cáo task, tuyệt đối không chỉnh sửa `.orchestrator/` hay `task.md`. |

## Điểm tôi không chắc chắn
- **Kích thước `telemetry_packet_t` (7 bytes vs 8 bytes)**: Tiêu đề mục 4.2 trong `knowledge.md` ghi `8 bytes, packed` và `task.md` cũng yêu cầu 8 bytes, nhưng khi tính tổng 5 trường được liệt kê trong struct thì chỉ có 7 bytes. Tôi đã tuân thủ nghiêm ngặt chỉ thị của `plan.md` là không chèn padding/reserved giả tạo, giữ nguyên 7 bytes. Cần người dùng/chủ dự án xác nhận đặc tả chính thức (có cần bổ sung 1 byte `reserved` hay giữ nguyên 7 bytes).
- **Môi trường build ESP-IDF thật**: Môi trường máy chủ hiện tại chưa cài đặt sẵn ESP-IDF toolchain (`idf.py`), do đó việc biên dịch `idf.py build` và nạp vào mạch thật chưa thực hiện trực tiếp trên máy này mà được xác minh qua các công cụ phân tích tĩnh, cấu hình CMake chuẩn và Python test vector.
