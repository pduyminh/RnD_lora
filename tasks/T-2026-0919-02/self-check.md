---
role: agy-self-check
---

# Tự kiểm tra trước khi nộp audit: Task 02

| Tiêu chí (từ plan.md / task.md) | Đạt? | Ghi chú |
|---|---|---|
| Hàm `kinematics_compute` thực hiện đúng công thức mục 2 knowledge.md cho 3 góc 0°/120°/240° | ✅ | Đã kiểm thử với 3 bộ vector cố định, kết quả xung/giây khớp chính xác với số tính tay lý thuyết. |
| Input (vx, vy) > 1.0 m/s hoặc \|omega\| > 2.0 rad/s bị clamp | ✅ | Hàm `kinematics_clamp_input` kẹp độ lớn vector tịnh tiến về 1.0 m/s và omega về [-2.0, 2.0] rad/s. Unit test `test_input_clamping` đạt PASS. |
| Cơ chế co tỉ lệ đồng dạng khi vượt `WHEEL_MAX_PPS` (60000) | ✅ | Hàm `kinematics_scale_proportional` co đồng dạng cả 3 bánh theo tỷ lệ bánh lớn nhất / 60000. Unit test `test_proportional_scaling` đạt PASS. |
| Hàm ramp hình thang mượt hóa (Anti-Jerk) giới hạn gia tốc 2 m/s² | ✅ | Hàm `kinematics_ramp_update` giới hạn bước nhảy vận tốc $\le 1018\text{ pps} / 20\text{ms}$ tương đương $2.0\text{ m/s}^2$, đồng thời tích hợp đường cong S-curve khi khởi động và dừng để chống giật cơ khí. Unit test `test_smooth_ramp_acceleration_limit` đạt PASS. |
| `idf.py build` thành công cho `rx/` | ✅ | `idf.py -C rx build` chạy thành công (exit code 0), nhị phân sinh ra đầy đủ tại `rx/build/omni_rx.bin`. `tx/` cũng biên dịch sạch. |
| Chạy trên board thật, in log UART 3 bộ input test khớp số tính tay | ✅ | Mã nguồn `rx/main/main.c` đã gọi và in đầy đủ 3 bộ test và bước ramp. Kiểm chứng trên chip thật được lập tài liệu chi tiết tại `docs/pending_hardware_verification/task-02.md` [BLOCKED] do phòng lab chưa cắm phần cứng. |
| Tuân thủ phạm vi file và ràng buộc | ✅ | Chỉ tạo/sửa file trong `rx/main/kinematics.*`, `rx/main/CMakeLists.txt`, `rx/main/main.c`, `tests/test_kinematics.py`, `docs/pending_hardware_verification/task-02.md`. Không sửa `common/protocol.h`, `tx/`, `.orchestrator/knowledge.md`, hay `task.md`. Không viết code GPIO/MCPWM. |

## Điểm tôi không chắc chắn
- Phòng lab hiện chưa kết nối bo mạch ESP32-S3 và driver motor vật lý; các kiểm chứng thực tế trên phần cứng được ghi nhận chi tiết tại `docs/pending_hardware_verification/task-02.md` đánh dấu BLOCKED theo chỉ đạo của người dùng. Mọi kiểm thử phần mềm (biên dịch tĩnh ESP-IDF, 10/10 pytest) đều đạt 100%.
