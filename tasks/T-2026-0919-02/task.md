---
task_id: T-2026-0919-02
created_by: user
created_at: 2026-09-19
status: open
priority: high
---

# Nhiệm vụ
Trên firmware `rx/`, viết module tính động học tam giác (kinematics) THUẦN
TÍNH TOÁN — không đụng tới GPIO/driver — theo đúng công thức mục 2 và giới
hạn tốc độ mục 3 trong `knowledge.md`. Module nhận (vx, vy, omega) và trả về
3 giá trị xung/giây có áp dụng co tỉ lệ khi vượt `wheel_max_pps`, cùng bộ
ramp hình thang giới hạn gia tốc.

Vì dự án không dùng môi trường mô phỏng (theo yêu cầu), việc kiểm tra thực
hiện bằng cách: firmware `rx` chạy một chuỗi input (vx, vy, omega) cố định
đã biết trước viết cứng trong code test, in kết quả pulses/s từng trục ra
UART để đối chiếu bằng tay với số tính sẵn (Excel/máy tính) — không cần
driver hay động cơ thật ở task này.

## Tiêu chí chấp nhận
- [ ] Có hàm `kinematics_compute(float vx, float vy, float omega, int32_t out_pps[3])`
      thực hiện đúng công thức mục 2 trong `knowledge.md` cho 3 góc bánh
      0°/120°/240°
- [ ] Input (vx, vy) có độ lớn > 1.0 m/s hoặc |omega| > 2.0 rad/s bị clamp
      về đúng giới hạn mục 3 trước khi tính, không vượt trần
- [ ] Có cơ chế co tỉ lệ: khi bất kỳ |out_pps[i]| vượt hằng số
      `WHEEL_MAX_PPS` (định nghĩa tạm = 60000, sẽ cập nhật sau T03), cả 3
      giá trị co theo cùng hệ số để giá trị lớn nhất đúng bằng `WHEEL_MAX_PPS`
- [ ] Có hàm ramp hình thang giới hạn tốc độ thay đổi pulses/s mỗi chu kỳ
      gọi (20 ms) tương ứng gia tốc tối đa 2 m/s² quy đổi theo mục 1.4
- [ ] `idf.py build` thành công cho `rx/`
- [ ] Chạy trên board thật, với ít nhất 3 bộ input test cố định viết trong
      code (ví dụ: vx=1,vy=0,omega=0 / vx=0,vy=0,omega=2 / vx=0.5,vy=0.5,omega=1),
      log UART in ra 3 giá trị pulses/s mỗi trục khớp với số tính tay được
      đính kèm trong `work-log.md`

## Ràng buộc
- Chỉ sửa/thêm file trong `rx/main/kinematics.c`, `rx/main/kinematics.h`,
  và file test tạm trong `rx/main/app_main.c` (chỉ phần gọi test, không đổi
  phần init đã có ở T01)
- Không được đụng tới `common/protocol.h`, `tx/`
- Không viết code điều khiển GPIO/MCPWM ở task này

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (mục 1.4, 2, 3)
