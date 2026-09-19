---
task_id: T-2026-0919-01
created_by: user
created_at: 2026-09-19
status: open
priority: high
---

# Nhiệm vụ
Khởi tạo khung dự án ESP-IDF gồm 2 target `tx/` và `rx/` cùng module `common/`
chứa định nghĩa giao thức truyền thông (`protocol.h`) theo đúng mục 4 của
`knowledge.md`. Đây là task nền tảng, chưa cần logic điều khiển động cơ hay
web server — chỉ cần build được, chạy được, và chứng minh struct đóng gói
đúng kích thước + CRC hoạt động đúng.

## Tiêu chí chấp nhận
- [ ] `idf.py build` chạy thành công (exit code 0) cho cả `tx/` và `rx/`
- [ ] `common/protocol.h` định nghĩa đúng `ctrl_packet_t` (12 bytes) và
      `telemetry_packet_t` (8 bytes) như mục 4.1, 4.2 trong `knowledge.md`,
      dùng `__attribute__((packed))`
- [ ] Có hàm `crc16_ccitt_false(const uint8_t *data, size_t len)` trong
      `common/protocol.h` (hoặc `.c` tương ứng), có ít nhất 1 test vector
      cố định (input/output đã biết trước) được in ra qua UART lúc boot để
      đối chiếu bằng mắt
- [ ] Cả hai firmware `tx` và `rx` khi flash lên board, in ra UART log dòng
      `sizeof(ctrl_packet_t) = 12` và `sizeof(telemetry_packet_t) = 8` —
      đúng bằng số byte quy định, không có padding thừa
- [ ] LED trạng thái (GPIO2) trên cả 2 board nhấp nháy 1 Hz để xác nhận
      firmware chạy, không treo

## Ràng buộc
- Chỉ tạo mới, không có code cũ để sửa
- Cấu trúc thư mục phải đúng theo mục 6 trong `knowledge.md`
- Không cài đặt ESP-NOW, WiFi, hay logic động cơ ở task này — để task sau
- Không sửa `knowledge.md`

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (đặc biệt mục 1.2, 4, 6)
