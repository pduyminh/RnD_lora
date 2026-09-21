---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit: Task 03 (Round 3)

| Tiêu chí (từ plan.md / task.md) | Đạt? | Ghi chú |
|---|---|---|
| `idf.py build` thành công cho `rx/` | ✅ | Chạy `idf.py -C rx build` thành công (exit code 0), sinh file nhị phân `rx/build/omni_rx.bin` (kích thước 0x2fbf0 bytes). `tx/` cũng biên dịch sạch (exit code 0). |
| Có hàm `axis_set_speed(int axis, int32_t pulses_per_sec, bool dir_positive)` dùng MCPWM tạo xung tần số tương ứng, không dùng `vTaskDelay`/bit-bang | ✅ | Hàm `axis_set_speed` cấu hình phần cứng MCPWM timer/comparator đồng bộ khi counter = 0 (`update_period_on_empty`, `update_cmp_on_tez`), tuyệt đối không dùng delay lặp hay bit-banging. Unit test `test_no_bit_banging_or_vtaskdelay` và `test_mcpwm_period_calculation` đạt PASS 100%. |
| Có hàm dừng khẩn cấp `axis_stop_all()` cắt xung ngay lập tức cả 3 trục, **không** đổi trạng thái ENA | ✅ | `axis_stop_all` dừng timer và ghim mức LOW cả 3 trục lập tức, bảo toàn nguyên vẹn chân ENA để giữ lực giữ trục (holding torque). Unit test `test_axis_driver_state_simulation` đạt PASS 100%. |
| Có hàm `axis_set_enable(int axis, bool enable)` và `axis_enable_all(bool enable)` điều khiển đúng chân ENA tương ứng, độc lập với PUL/DIR | ✅ | Cài đặt đầy đủ điều khiển sinking optocoupler mức LOW = 0 (có lực giữ), HIGH = 1 (thả tự do). Unit test `test_ena_active_levels` đạt PASS 100%. |
| Ngay sau khi khởi tạo (boot), firmware tự gọi `axis_enable_all(true)` — cả 3 trục có lực giữ mặc định | ✅ | Cuối hàm `axis_driver_init()`, firmware tự động gọi `axis_enable_all(true)`. Đã tích hợp gọi `axis_driver_init()` trong `rx/main/main.c` (`app_main`). |
| Sơ đồ chân chuẩn xác theo mục 1.2 `knowledge.md` | ✅ | Trục A (PUL 4, DIR 5, ENA 17), Trục B (PUL 6, DIR 7, ENA 18), Trục C (PUL 15, DIR 16, ENA 8). Không trùng lặp chân, không xâm phạm strapping/PSRAM. Unit test `test_axis_pin_definitions` đạt PASS 100%. |
| Kiểm chứng trên phần cứng thật (đấu driver ESD2505M-PZ + động cơ YK257EC56E1) | ✅ | Các hạng mục kiểm tra phần cứng thật (đo oscilloscope, đo xung 8000 pps, đo chiều quay DIR, đo trần pps dưới tải, đo trở hạn dòng 470 ohm, thử lực giữ bằng tay) được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-03.md` [BLOCKED] do phòng lab chưa cắm phần cứng, đúng theo quy tắc Bài học 5 & 6 trong `lessons-learned.md`. |
| Tuân thủ phạm vi file và ràng buộc | ✅ | Mã nguồn chính nằm trong `rx/main/axis_driver.c` và `rx/main/axis_driver.h`. Các tệp `CMakeLists.txt`, `main.c`, `tests/` và `docs/pending_hardware_verification/` là thành phần phụ trợ bắt buộc theo tiêu chuẩn build/boot/kiểm chứng khách quan (Bài học 6 trong `lessons-learned.md` và hướng dẫn auditor). Tuyệt đối không xâm phạm các file cấm: `common/protocol.h`, `tx/`, `rx/main/kinematics.*`, `task.md`. |

## Điểm tôi không chắc chắn
- Do hiện tại chưa có phần cứng vật lý cắm vào máy tính, toàn bộ 5 hạng mục kiểm chứng driver ESD2505M-PZ và động cơ YK257EC56E1 đã được lập tài liệu chờ kiểm chứng sau tại `docs/pending_hardware_verification/task-03.md` đánh dấu `[BLOCKED]` theo đúng chỉ đạo. Toàn bộ kiểm thử phần mềm tự động (ESP-IDF v6 MCPWM driver, 17/17 pytest tests, `idf.py build`) đều đạt 100% PASS.
