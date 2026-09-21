---
task_id: T-2026-0919-03
created_by: user
created_at: 2026-09-19
status: open
priority: high
---

# Nhiệm vụ
Trên firmware `rx/`, dùng MCPWM của ESP-IDF để sinh xung PUL/DIR độc lập
cho 3 trục theo sơ đồ chân mục 1.2 trong `knowledge.md`, đồng thời điều
khiển 3 chân ENA độc lập theo chính sách mục 1.3.1. Task này BẮT BUỘC
có phần kiểm tra thủ công trên phần cứng thật (đấu driver + động cơ thật)
vì đây là lần đầu đưa tín hiệu ra driver ESD2505M-PZ thật — không thể xác
nhận bằng log phần mềm đơn thuần.

## Tiêu chí chấp nhận (tự động — kiểm bằng build)
- [ ] `idf.py build` thành công cho `rx/`
- [ ] Có hàm `axis_set_speed(int axis /*0,1,2*/, int32_t pulses_per_sec, bool dir_positive)`
      dùng MCPWM tạo xung tần số tương ứng, không dùng `vTaskDelay`/bit-bang
- [ ] Có hàm dừng khẩn cấp `axis_stop_all()` cắt xung ngay lập tức cả 3 trục,
      **không** đổi trạng thái ENA (giữ nguyên lực giữ trục)
- [ ] Có hàm `axis_set_enable(int axis, bool enable)` và `axis_enable_all(bool enable)`
      điều khiển đúng 1 trong 3 chân ENA tương ứng, độc lập với PUL/DIR
- [ ] Ngay sau khi khởi tạo (boot), firmware tự gọi `axis_enable_all(true)` —
      cả 3 trục có lực giữ mặc định, kể cả khi chưa nhận lệnh di chuyển nào

## Tiêu chí chấp nhận (kiểm tra thủ công trên phần cứng thật — ghi kết quả vào work-log.md)
- [ ] Đấu 1 trục với driver ESD2505M-PZ và động cơ YK257EC56E1 thật, xác
      nhận mức tín hiệu ra GPIO đủ để driver nhận biết ổn định (đo bằng
      đồng hồ đo hoặc quan sát driver chạy êm không rung/kêu bất thường)
- [ ] Với lệnh `pulses_per_sec` cố định (ví dụ 8000 = 1 vòng/giây), đo tốc
      độ quay thực tế bằng mắt/đồng hồ bấm giờ hoặc oscilloscope, xác nhận
      khớp với 8000 xung/vòng đã cấu hình DIP driver
- [ ] Xác định chiều quay thực tế ứng với DIR=HIGH cho CẢ 3 trục, điền kết
      quả vào bảng "Quy ước chiều quay" mục 1.5 của `knowledge.md` (đây là
      trường hợp ngoại lệ được phép sửa `knowledge.md`, phải ghi rõ trong
      `work-log.md` lý do và giá trị cụ thể đã điền)
- [ ] Đo tốc độ xung/giây tối đa mỗi trục chạy được liên tục dưới tải
      (gắn bánh, đẩy tay nhẹ để mô phỏng tải) mà không mất bước/kêu rít bất
      thường trong ít nhất 30 giây liên tục — ghi số liệu này là
      `wheel_max_pps` thực đo vào mục 3 của `knowledge.md`
- [ ] Xác nhận điện trở hạn dòng 470 Ω đề xuất trong mục 1.3 `knowledge.md`
      cho ra dòng qua opto nằm trong khoảng driver yêu cầu (đo bằng đồng
      hồ vạn năng hoặc đối chiếu datasheet thật của ESD2505M-PZ nếu có) —
      nếu sai lệch, cập nhật lại giá trị điện trở và ghi rõ trong `work-log.md`
- [ ] Với từng trục: đảo mức GPIO điều khiển ENA và quan sát bằng tay (cố
      xoay trục bằng tay) xem trục có lực giữ (cứng, khó xoay) hay thả tự
      do (xoay nhẹ tay) — xác định mức nào là "enable", điền kết quả vào
      bảng ENA mục 1.3.1 của `knowledge.md` cho cả 3 trục (trường hợp ngoại
      lệ được phép sửa `knowledge.md`, ghi rõ lý do trong `work-log.md`)

## Ràng buộc
- Chỉ sửa/thêm file trong `rx/main/axis_driver.c`, `rx/main/axis_driver.h`
- Được phép sửa duy nhất các vị trí sau trong `knowledge.md`: bảng chiều
  quay mục 1.5, giá trị `wheel_max_pps` mục 3, giá trị điện trở và bảng
  mức tích cực ENA mục 1.3/1.3.1 — không sửa gì khác trong file này
- Không đụng `tx/`, `common/protocol.h`, `rx/main/kinematics.*`

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (mục 1.2, 1.3, 1.5, 3)
