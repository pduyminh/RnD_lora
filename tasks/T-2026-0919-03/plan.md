---
role: codex-advisor
task_id: T-2026-0919-03
round: 1
---

# Khuyến nghị thực thi: Task 03 — RX MCPWM Pulse & ENA Axis Driver

## Cách tiếp cận đề xuất
1. Cài đặt module driver điều khiển xung và chiều trong `rx/main/axis_driver.h` và `rx/main/axis_driver.c` sử dụng hệ thống timer phần cứng MCPWM (`esp_driver_mcpwm`) của ESP-IDF v6.
2. Ánh xạ chính xác sơ đồ chân mục 1.2 knowledge.md:
   - Trục A: PUL GPIO 4, DIR GPIO 5, ENA GPIO 17
   - Trục B: PUL GPIO 6, DIR GPIO 7, ENA GPIO 18
   - Trục C: PUL GPIO 15, DIR GPIO 16, ENA GPIO 8
3. Cấu hình MCPWM timer group 0 với 3 timer/oper/cmpr/gen độc lập, resolution 1 MHz (1 tick = 1 us). Tần số xung thay đổi qua cập nhật period ticks và compare ticks (50% duty cycle) đồng bộ khi counter = 0 (`update_period_on_empty`, `update_cmp_on_tez`) để tạo xung cực mịn, không bị giật hay méo chu kỳ xung.
4. Điều khiển chân ENA độc lập: Mức LOW = 0 (kích hoạt opto sinking = có lực giữ khóa cứng động cơ), Mức HIGH = 1 (thả tự do).
5. Khởi tạo mặc định lúc boot: Firmware tự động gọi `axis_enable_all(true)` để cấp lực giữ tức thì cho cả 3 trục ngay cả khi robot chưa di chuyển.
6. Cung cấp hàm dừng khẩn cấp `axis_stop_all()`: Dừng phát xung ngay lập tức (force level LOW) nhưng KHÔNG đổi trạng thái ENA để giữ nguyên mô-men khóa vị trí trục.
7. Cập nhật `rx/main/CMakeLists.txt` (bổ sung `axis_driver.c` và dependency `esp_driver_mcpwm`), `rx/main/main.c` (gọi `axis_driver_init` lúc boot).
8. Xây dựng unit test kiểm thử `tests/test_axis_driver.py` và tài liệu kiểm chứng phần cứng còn chờ `docs/pending_hardware_verification/task-03.md` [BLOCKED].

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `rx/main/axis_driver.h`, `rx/main/axis_driver.c`, `rx/main/CMakeLists.txt`, `rx/main/main.c`, `tests/test_axis_driver.py`, `docs/pending_hardware_verification/task-03.md`, các file nhật ký trong `tasks/T-2026-0919-03/`.
- CẤM sửa `common/protocol.h`, `tx/`, `rx/main/kinematics.*`, `task.md`.
- CẤM dùng `vTaskDelay` hoặc bit-banging cho việc sinh xung động cơ.
- Do phòng lab chưa cắm phần cứng thật, toàn bộ các hạng mục kiểm chứng phần cứng thật với driver ESD2505M-PZ và động cơ YK257EC56E1 phải được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-03.md` với trạng thái `[BLOCKED]`.

## Rủi ro cần lưu ý
- Cấu hình MCPWM của ESP-IDF v6: Dùng header `driver/mcpwm_prelude.h` và component `esp_driver_mcpwm`.
- Tần số xung tối thiểu / tối đa: Khi pps <= 0 cần ngắt xung bằng cách stop timer và ép mức LOW. Khi pps cao (lên tới 60000 pps), chu kỳ khoảng 16 us vẫn nằm trong ngưỡng phân giải 1 us của MCPWM.
- Bảo toàn ENA trong E-stop: Tuyệt đối không gọi tắt ENA khi E-stop để tránh xe trôi dốc hoặc mất vị trí.

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- `idf.py -C rx build` trả exit code 0.
- `python -m pytest tests -v --tb=short` pass 100% (17/17 tests).
- Có đầy đủ các hàm API: `axis_driver_init`, `axis_set_speed`, `axis_stop_all`, `axis_set_enable`, `axis_enable_all`.
- Có tài liệu kiểm chứng phần cứng [BLOCKED] cho task-03.
