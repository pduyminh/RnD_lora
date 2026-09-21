---
role: codex-advisor
task_id: T-2026-0919-02
round: 1
---

# Khuyến nghị thực thi

## Cách tiếp cận đề xuất
1. Cài đặt module động học tam giác trong `rx/main/kinematics.h` và `rx/main/kinematics.c`.
2. Áp dụng đúng công thức động học nghịch mục 2 knowledge.md cho 3 bánh omni tại góc 0°, 120°, 240° với L=0.38m, r=0.05m và tỷ lệ 25464.79 pulses/m.
3. Kẹp trần đầu vào theo mục 3 knowledge.md: magnitude của vector tịnh tiến (vx, vy) <= 1.0 m/s (co tỉ lệ nếu vượt), và |omega| <= 2.0 rad/s.
4. Triển khai cơ chế co tỉ lệ đồng dạng cả 3 giá trị pulses/s khi có bánh vượt WHEEL_MAX_PPS (60000).
5. Xây dựng giải thuật ramp gia tốc hình thang giới hạn gia tốc 2.0 m/s^2 (tương đương 1018 pulses/s mỗi chu kỳ 20ms) kết hợp mượt hóa Anti-Jerk S-curve khi xuất phát và khi hãm dừng để chống giật cơ khí.
6. Cập nhật `rx/main/main.c` để chạy và in log 3 bộ input test cố định cùng bước ramp đầu tiên.
7. Xây dựng bộ test độc lập `tests/test_kinematics.py` và tài liệu kiểm chứng phần cứng còn chờ `docs/pending_hardware_verification/task-02.md` [BLOCKED].

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `rx/main/kinematics.h`, `rx/main/kinematics.c`, `rx/main/CMakeLists.txt`, `rx/main/main.c`, `tests/test_kinematics.py`, `docs/pending_hardware_verification/task-02.md`, các file nhật ký trong `tasks/T-2026-0919-02/`.
- CẤM sửa `common/protocol.h`, `tx/`, `.orchestrator/knowledge.md`, `task.md`.
- CẤM viết code điều khiển GPIO/MCPWM ở task này.

## Rủi ro cần lưu ý
- Góc bánh xe và dấu lượng giác: Bánh 0 tại 0° chỉ chịu ảnh hưởng vy và L*omega. Bánh 1 tại 120° có sin(120°)=sqrt(3)/2, cos(120°)=-0.5. Bánh 2 tại 240° có sin(240°)=-sqrt(3)/2, cos(240°)=-0.5.
- Làm tròn số thực sang xung nguyên: Dùng hàm `lrintf` chuẩn để tránh sai số lũy kế.
- Không có phần cứng thật: Ghi nhận kiểm chứng log UART và LED tại `docs/pending_hardware_verification/task-02.md` [BLOCKED].

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- `idf.py -C rx build` trả exit code 0.
- `python -m pytest tests -v --tb=short` pass 100%.
- Có đầy đủ 3 bộ test vector và kết quả tính khớp chính xác.
- Có tài liệu kiểm chứng phần cứng [BLOCKED] cho task-02.
