# Tự kiểm tra (Self-check) — Task 06 (TM1638 Display & Physical E-Stop)

## 1. Đối chiếu tiêu chí chấp nhận

| Tiêu chí | Trạng thái | Bằng chứng kiểm chứng |
|---|---|---|
| `idf.py build` thành công cho `tx/` | PASS | Biên dịch `idf.py -C tx build` thành công, sinh nhị phân `omni_tx.bin` (exit code 0). |
| Driver TM1638 giao tiếp 3 dây, không dùng Arduino | PASS | Driver bit-banging thuần ESP-IDF trong `tx/main/tm1638.c` sử dụng `driver/gpio.h`, `esp_rom_sys.h`, không có bất kỳ thư viện Arduino nào. |
| Bấm nút số 1 → đặt cờ `estop=1` gửi ngay, chốt trạng thái cho đến khi bấm lại | PASS (Logic phần mềm: PASS, Phần cứng: [BLOCKED]) | Phát hiện cạnh lên nút 1, đảo trạng thái `s_tm1638_estop`, gọi `tx_send_packet_now()` tức thì; được kiểm chứng qua `test_button1_estop_toggle_and_latch_logic` trong `tests/test_tm1638.py`. |
| Hiển thị lên 8 LED 7 đoạn: AP client count & vx (mm/s) | PASS | Vị trí 0-1 hiển thị `'C'` + client count, vị trí 3-7 hiển thị $v_x$ dạng số nguyên mm/s. Kiểm chứng qua `test_tm1638_display_formatting` và `test_tm1638_seven_segment_encoding_logic`. |
| LED đơn thứ nhất sáng khi estop, tắt khi bình thường | PASS | LED 0 (LED 1 trên module) được đồng bộ với cờ `estop`: sáng khi estop=true, tắt khi estop=false. Kiểm chứng trong `test_button1_estop_toggle_and_latch_logic`. |
| Kiểm tra thủ công RX dừng ngay không ramp khi bấm E-Stop | [BLOCKED] | Chưa có phần cứng thật để bấm tay; quy trình chi tiết đã được lập tại `docs/pending_hardware_verification/task-06.md` theo đúng quy định. |
| Toàn bộ unit test (pytest) | PASS | Chạy toàn bộ test suite `pytest tests -v --tb=short` đạt 100% (39/39 tests). |

## 2. Ràng buộc file
- Không đụng: `rx/`, `common/protocol.h`, `knowledge.md`.
- Các thay đổi bổ trợ (`CMakeLists.txt`, `main.c`, `tx_server.*`, `tests/*`, `docs/*`) đều là file tích hợp và kiểm chứng hợp lệ đã nêu trong `plan.md`.
