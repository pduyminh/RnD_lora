---
task_id: T-2026-0919-04
created_by: user
created_at: 2026-09-19
status: open
priority: high
---

# Nhiệm vụ
Ghép nối 3 phần đã có của `rx/` (protocol từ T01, kinematics từ T02, axis
driver từ T03) thành pipeline hoàn chỉnh: nhận `ctrl_packet_t` qua ESP-NOW
→ validate (magic, CRC, seq mới hơn) → kinematics → axis driver, kèm
failsafe theo mục 4.3 `knowledge.md`. Đồng thời gửi `telemetry_packet_t`
ngược lại địa chỉ MAC nguồn ở mỗi lần nhận hợp lệ.

## Tiêu chí chấp nhận
- [ ] `idf.py build` thành công cho `rx/`
- [ ] Gói nhận sai `magic` hoặc sai `crc16` bị loại bỏ hoàn toàn, không đưa
      vào kinematics (log UART đếm số gói bị loại)
- [ ] Gói có `seq` nhỏ hơn hoặc bằng `seq` đã xử lý gần nhất bị loại (chống
      gói cũ/lặp)
- [ ] Nhận `estop=1` → gọi `axis_stop_all()` ngay trong vòng lặp xử lý gói
      đó, không đợi ramp; **không gọi `axis_set_enable`/`axis_enable_all`
      để tắt ENA** — lực giữ trục phải được giữ nguyên theo đúng chính
      sách mục 1.3.1 `knowledge.md`
- [ ] Không nhận được gói hợp lệ nào trong 300 ms liên tục → tự động ramp
      giảm tốc cả 3 trục về 0 theo gia tốc tối đa đã định trong T02, log rõ
      dòng "FAILSAFE TRIGGERED" ra UART; tương tự estop, **giữ nguyên ENA
      ở trạng thái enable** trong suốt và sau khi failsafe kích hoạt
- [ ] Sau mỗi gói lệnh hợp lệ, gửi lại `telemetry_packet_t` với `seq_echo`
      đúng bằng seq vừa nhận và `link_ok=1`
- [ ] Kiểm tra thủ công: dùng 1 thiết bị ESP32 khác (hoặc T05 nếu đã xong)
      gửi liên tục gói lệnh cố định, xác nhận cả 3 trục quay đúng theo
      công thức kinematics đã kiểm ở T02, và khi tắt nguồn thiết bị gửi,
      RX dừng êm trong khoảng 300ms–vài trăm ms sau đó (không dừng khựng đột ngột)

## Ràng buộc
- Chỉ sửa/thêm file trong `rx/main/comm.c`, `rx/main/comm.h`,
  `rx/main/app_main.c` (phần ghép nối pipeline)
- Không sửa lại logic bên trong `kinematics.c`, `axis_driver.c`,
  `common/protocol.h` đã có — nếu phát hiện lỗi ở các file đó, ghi vào
  `work-log.md` phần "Quyết định khác với plan" thay vì tự sửa
- Không sửa `knowledge.md`
- Không đụng `tx/`

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (mục 4.3)
