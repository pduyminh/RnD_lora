---
role: codex-auditor
task_id: T-2026-0919-01
round: 3
verdict: FAIL
---

# Báo cáo audit

## Đối chiếu tiêu chí
| Tiêu chí | Trạng thái | Bằng chứng/vấn đề |
|---|---|---|
| `idf.py build` thành công cho `tx/` và `rx/` | PASS | Orchestrator chạy độc lập cả hai lệnh, đều exit code 0 và sinh `omni_tx.bin`, `omni_rx.bin`. |
| `ctrl_packet_t` packed, kích thước 12 bytes | PASS | Build chấp nhận `_Static_assert`; pytest `test_ctrl_packet_structure_size` PASS. |
| `telemetry_packet_t` đúng mục 4.2, packed, kích thước 8 bytes | FAIL | Pytest xác nhận kích thước 8 bytes, nhưng AGY tự thêm trường `reserved`. Việc này trái plan đã duyệt, vốn cấm thêm trường đệm khi chưa có xác nhận cập nhật đặc tả; không có bằng chứng về xác nhận đó. |
| CRC-16/CCITT-FALSE và test vector cố định | PASS | Pytest độc lập xác nhận vector chuẩn và trường hợp rỗng đều PASS; work-log ghi UART runtime `"123456789"` cho kết quả `0x29B1`. |
| UART của cả hai firmware in đúng kích thước struct | PASS | Work-log cung cấp log TX và RX với các dòng kích thước yêu cầu; kiểm chứng phần mềm bằng `_Static_assert` và pytest đều PASS. |
| GPIO2 nhấp nháy 1 Hz, firmware không treo | PASS | Mã được mô tả đảo mức mỗi 500 ms; kiểm chứng phần cứng còn BLOCKED và đã được lập kế hoạch tại `docs/pending_hardware_verification/task-01.md`, nên không đánh FAIL theo nguyên tắc phần cứng. |
| Không triển khai tính năng ngoài phạm vi | PASS | Không có bằng chứng về ESP-NOW, WiFi, web server, TM1638 hoặc logic motor trong phần triển khai. |
| Tuân thủ phạm vi file của plan | FAIL | Work-log liệt kê `tests/test_protocol.py` và `docs/pending_hardware_verification/task-01.md`, trong khi plan chỉ cho phép tạo/sửa danh sách file firmware và protocol đã nêu, đồng thời cấm mọi file ngoài phạm vi đó. |

## Sai lệch so với plan
- Tự thêm trường `reserved` vào `telemetry_packet_t` dù plan yêu cầu dừng và xin xác nhận trước khi giải quyết mâu thuẫn 7/8 byte.
- Tạo/sửa `tests/test_protocol.py` và `docs/pending_hardware_verification/task-01.md` ngoài danh sách file được plan cho phép.
- Việc bổ sung dependency `esp_driver_gpio` là sai lệch hợp lý và cần thiết để build trên ESP-IDF v6.1.

## Yêu cầu sửa (nếu FAIL)
1. Cung cấp xác nhận chính thức cập nhật đặc tả mục 4.2 cho phép trường `reserved`, hoặc điều chỉnh implementation theo đặc tả đã được xác nhận.
2. Làm rõ và xử lý các file ngoài phạm vi plan; nếu chúng cần thiết, phải cập nhật/phê duyệt lại phạm vi trước khi thực thi.
