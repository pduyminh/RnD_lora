---
role: codex-auditor
task_id: T-2026-0919-05
round: 1
verdict: PASS_WITH_NOTES
---

# Báo cáo audit

## Đối chiếu tiêu chí
| Tiêu chí | Trạng thái | Bằng chứng/vấn đề |
|---|---|---|
| `idf.py build` thành công cho `tx/` | PASS | Orchestrator chạy `idf.py -C tx build`, exit code 0; sinh `omni_tx.bin`. |
| SoftAP có SSID/password cấu hình tập trung | PASS | Macro `TX_AP_SSID` và `TX_AP_PASSWORD` được kiểm tra bởi `test_softap_and_timing_configurations`; test PASS. |
| Phục vụ trang HTML khi kết nối AP | PASS | Route và HTML nhúng chứa joystick, điều khiển omega và E-Stop; `test_embedded_html_contains_required_controls` PASS. Kiểm chứng trên trình duyệt thật đang BLOCKED do thiếu phần cứng. |
| HTML gửi WebSocket và TX cập nhật vx/vy/omega | PASS | Handler `/ws` và cập nhật trạng thái được kiểm tra bởi `test_normal_motion_with_connected_client`; PASS. |
| Task ESP-NOW độc lập chạy 20 ms, seq tăng 1, CRC đúng | PASS | TX build thành công; test cấu hình chu kỳ và `test_seq_increments_by_one_every_packet` PASS; CRC dùng `ctrl_packet_calc_crc`, đồng thời test protocol PASS. Độ lệch thời gian thực trên phần cứng chưa đo được. |
| Không có client thì gửi vx=vy=omega=0 | PASS | `test_zero_motion_when_no_client` và `test_client_timeout_zeroes_motion` đều PASS. |
| Kiểm tra thủ công bằng AP, trình duyệt và UART RX | PASS | Đã lập quy trình chi tiết tại `docs/pending_hardware_verification/task-05.md` với trạng thái `[BLOCKED]`; được chấp nhận theo quy tắc audit khi chưa có phần cứng và toàn bộ kiểm chứng phần mềm đều PASS. |
| Toàn bộ unit test | PASS | Orchestrator chạy 31 test, kết quả 31/31 PASS, exit code 0. |
| Tuân thủ phạm vi và không sửa file cấm | PASS | Work-log ghi nhận không sửa `rx/`, `common/protocol.h`, `knowledge.md`; các file cấu hình, build integration, test và tài liệu kiểm chứng thuộc ngoại lệ hợp lệ đã được plan phê duyệt. |
| Kiểm tra định dạng diff | PASS | `git diff --check` exit code 0. |

## Sai lệch so với plan
- Không có sai lệch chức năng đáng kể.
- Kiểm chứng vật lý SoftAP/WebSocket, chu kỳ 20 ms ± 2 ms và log UART RX chưa thể thực hiện do phòng lab chưa cắm phần cứng; đã có tài liệu `[BLOCKED]` đúng yêu cầu.
- Build có một số cảnh báo môi trường ESP-IDF/CMake và thiếu `ESP_ROM_ELF_DIR`, nhưng không làm thất bại quá trình build; cả TX và RX đều trả exit code 0.

## Yêu cầu sửa (nếu FAIL)
1. Không có.