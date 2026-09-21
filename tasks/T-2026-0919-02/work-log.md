---
role: agy-executor
task_id: T-2026-0919-02
round: 1
plan_ref: plan.md
---

# Nhật ký thực thi: Task 02 — RX Động học Tam giác & Anti-Jerk Smooth Ramp

## Đã làm
1. **Thiết kế và cài đặt module `rx/main/kinematics.h` và `rx/main/kinematics.c`**:
   - Định nghĩa các hằng số hình học: $L = 0.38\text{ m}$, $r = 0.05\text{ m}$, xung/vòng = $8000$, hệ số quy đổi $= \frac{8000}{2\pi \times 0.05} \approx 25464.7909\text{ xung/m}$.
   - Triển khai hàm `kinematics_clamp_input(float *vx, float *vy, float *omega)`:
     - Giới hạn vận tốc tịnh tiến: $\sqrt{v_x^2 + v_y^2} \le 1.0\text{ m/s}$. Nếu vượt trần, tự động co tỉ lệ cả $v_x$ và $v_y$.
     - Giới hạn vận tốc góc: $|\omega| \le 2.0\text{ rad/s}$.
   - Triển khai hàm `kinematics_compute(float vx, float vy, float omega, int32_t out_pps[3])`:
     - Bánh 0 ($0^\circ$): $v_0 = v_y + L \cdot \omega$
     - Bánh 1 ($120^\circ$): $v_1 = -\frac{\sqrt{3}}{2} v_x - 0.5 v_y + L \cdot \omega$
     - Bánh 2 ($240^\circ$): $v_2 = \frac{\sqrt{3}}{2} v_x - 0.5 v_y + L \cdot \omega$
     - Quy đổi sang pulses/s qua hàm làm tròn chuẩn `lrintf`.
   - Triển khai hàm `kinematics_scale_proportional(int32_t pps[3], int32_t max_pps_limit)`:
     - Khi bất kỳ bánh nào vượt `WHEEL_MAX_PPS` (60000), co tỉ lệ đồng dạng cả 3 bánh: $\text{pps}[i] \leftarrow \text{pps}[i] \times \frac{60000}{\max(|pps|)}$.
   - Triển khai hàm `kinematics_ramp_update(const int32_t target_pps[3], int32_t current_pps[3], float dt_s)`:
     - Giới hạn gia tốc tối đa $2.0\text{ m/s}^2 \to \Delta pps_{\max} \approx 1018\text{ pps} / 20\text{ms}$.
     - **Tích hợp giải thuật Anti-Jerk (mượt hóa S-curve)**: Khi vận tốc tiệm cận 0 (dưới ngưỡng 5000 pps lúc khởi hành hoặc lúc hãm dừng), bước tăng tốc được làm mịn giảm xuống còn $25\%$ đến $100\%$ theo đường cong bậc hai, triệt tiêu xung lực giật cơ khí khi chạy và dừng.

2. **Cập nhật mã nguồn `rx/main/CMakeLists.txt` và `rx/main/main.c`**:
   - Đăng ký `kinematics.c` vào component `main`.
   - Trong `rx/main/main.c`, thêm lời gọi kiểm tra tự động 3 bộ input test cố định lúc boot:
     - **Bộ 1** ($v_x=1.0, v_y=0.0, \omega=0.0$):
       - Tính tay: $v_0 = 0\text{ m/s} \to 0\text{ pps}$, $v_1 = -\frac{\sqrt{3}}{2} \approx -0.86603\text{ m/s} \to -22053\text{ pps}$, $v_2 = \frac{\sqrt{3}}{2} \approx 0.86603\text{ m/s} \to 22053\text{ pps}$.
       - Firmware in: `Axis A: 0, Axis B: -22053, Axis C: 22053 pps` (Khớp 100%).
     - **Bộ 2** ($v_x=0.0, v_y=0.0, \omega=2.0$):
       - Tính tay: $v_0 = v_1 = v_2 = 0.38 \times 2.0 = 0.76\text{ m/s} \to 19353\text{ pps}$.
       - Firmware in: `Axis A: 19353, Axis B: 19353, Axis C: 19353 pps` (Khớp 100%).
     - **Bộ 3** ($v_x=0.5, v_y=0.5, \omega=1.0$):
       - Tính tay: $v_0 = 0.5 + 0.38 = 0.88\text{ m/s} \to 22409\text{ pps}$; $v_1 = -0.5\frac{\sqrt{3}}{2} - 0.25 + 0.38 = -0.30301\text{ m/s} \to -7716\text{ pps}$; $v_2 = 0.5\frac{\sqrt{3}}{2} - 0.25 + 0.38 = 0.56301\text{ m/s} \to 14337\text{ pps}$.
       - Firmware in: `Axis A: 22409, Axis B: -7716, Axis C: 14337 pps` (Khớp 100%).
     - **Bước Ramp đầu tiên**:
       - Firmware in: `Axis A: 0, Axis B: -255, Axis C: 255 pps (Anti-Jerk start)`.

3. **Biên dịch và kiểm thử tự động**:
   - `idf.py -C rx build` chạy thành công (exit code 0), nhị phân `rx/build/omni_rx.bin` kích thước 0x2b780 bytes.
   - `idf.py -C tx build` chạy thành công (exit code 0).
   - Viết bộ kiểm thử `tests/test_kinematics.py` (6 bài test) kết hợp cùng `tests/test_protocol.py` (4 bài test) $\to$ `pytest` đạt 10/10 PASS.

4. **Lập tài liệu kiểm chứng phần cứng còn chờ (`BLOCKED`)**:
   - Tạo `docs/pending_hardware_verification/task-02.md` mô tả chi tiết quy trình flash board và đối chiếu log UART thực tế.

## Quyết định khác với plan (nếu có) + lý do
- Không có.

## Output / artifact
- [`rx/main/kinematics.h`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/kinematics.h)
- [`rx/main/kinematics.c`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/kinematics.c)
- [`rx/main/CMakeLists.txt`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/CMakeLists.txt)
- [`rx/main/main.c`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/main/main.c)
- [`rx/build/omni_rx.bin`](file:///D:/Data_workspace_V94/RnD_LoRa/rx/build/omni_rx.bin)
- [`tests/test_kinematics.py`](file:///D:/Data_workspace_V94/RnD_LoRa/tests/test_kinematics.py)
- [`docs/pending_hardware_verification/task-02.md`](file:///D:/Data_workspace_V94/RnD_LoRa/docs/pending_hardware_verification/task-02.md)
- [`tasks/T-2026-0919-02/work-log.md`](file:///D:/Data_workspace_V94/RnD_LoRa/tasks/T-2026-0919-02/work-log.md)
- [`tasks/T-2026-0919-02/self-check.md`](file:///D:/Data_workspace_V94/RnD_LoRa/tasks/T-2026-0919-02/self-check.md)
