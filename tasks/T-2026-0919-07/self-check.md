# Tự kiểm tra (Self-check) — Task 07 (TX Telemetry Reception & Dual-Display Alert)

## 1. Đối chiếu tiêu chí chấp nhận

| Tiêu chí | Trạng thái | Bằng chứng kiểm chứng |
|---|---|---|
| `idf.py build` thành công cho `tx/` | PASS | Biên dịch `idf.py -C tx build` thành công, sinh `omni_tx.bin` (exit code 0). |
| TX nhận `telemetry_packet_t` qua ESP-NOW, validate `magic` + `crc16`, loại bỏ gói sai | PASS | `telemetry_process_packet()` kiểm tra `magic == 0x5A` và tính CRC-16; được kiểm chứng qua `test_telemetry_packet_validation` trong `tests/test_telemetry.py`. |
| Nếu không nhận được telemetry hợp lệ trong 500 ms → coi là "mất kết nối RX" | PASS | Macro `TELEMETRY_TIMEOUT_MS = 500` được định nghĩa rõ ràng; máy trạng thái được kiểm chứng qua `test_telemetry_timeout_definition` và `test_telemetry_timeout_and_recovery_state_machine`. |
| Trạng thái liên kết đẩy qua WebSocket tới mọi client trình duyệt, cập nhật thời gian thực | PASS | Hàm `tx_broadcast_ws_message()` đẩy bản tin JSON; trang HTML có thẻ `#rx-status` và callback `ws.onmessage`. Kiểm chứng qua `test_websocket_broadcast_status_formatting` và `test_index_html_contains_rx_status_and_ws_listener`. |
| LED đơn thứ hai trên TM1638 sáng khi mất kết nối RX, tắt khi bình thường | PASS | Cập nhật LED 2 (index 1) qua `tm1638_set_led(1, !telemetry_is_link_ok())`. Kiểm chứng qua `test_tm1638_led2_indication`. |
| Kiểm tra thủ công tắt/bật nguồn RX trong < 1s | [BLOCKED] | Chưa có phần cứng thật để thử nghiệm rút điện; quy trình chi tiết đã được lập tại `docs/pending_hardware_verification/task-07.md` theo đúng quy định. |
| Toàn bộ unit test (pytest) | PASS | Toàn bộ 47/47 test của toàn dự án đều PASS 100% trong 0.37s. |

## 2. Ràng buộc file
- Không đụng: `rx/`, `common/protocol.h`, `knowledge.md`.
- Các thay đổi bổ trợ (`CMakeLists.txt`, `main.c`, `tx_server.*`, `tm1638.c`, `tests/*`, `docs/*`) đều là file tích hợp và kiểm chứng hợp lệ đã nêu trong `plan.md`.
