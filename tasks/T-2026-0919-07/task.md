---
task_id: T-2026-0919-07
created_by: user
created_at: 2026-09-19
status: open
priority: low
---

# Nhiệm vụ
Nhận `telemetry_packet_t` mà RX gửi ngược (đã làm ở T04) trên TX, hiển thị
trạng thái liên kết lên cả trang web (qua WebSocket ngược lại trình duyệt)
và lên TM1638 (đã làm ở T06).

## Tiêu chí chấp nhận
- [ ] `idf.py build` thành công cho `tx/`
- [ ] TX nhận `telemetry_packet_t` qua ESP-NOW, validate `magic` + `crc16`,
      loại bỏ gói sai
- [ ] Nếu không nhận được telemetry hợp lệ nào trong 500 ms (lớn hơn
      failsafe timeout 300ms của RX để tránh báo giả), TX coi là "mất kết
      nối RX" và cập nhật trạng thái tương ứng
- [ ] Trạng thái liên kết ("OK" / "MẤT KẾT NỐI") được đẩy qua WebSocket tới
      mọi client trình duyệt đang mở trang, cập nhật hiển thị theo thời gian
      thực (không cần refresh trang)
- [ ] LED đơn thứ hai trên TM1638 sáng khi mất kết nối RX, tắt khi bình thường
- [ ] Kiểm tra thủ công: tắt nguồn RX giữa lúc đang chạy, xác nhận trong
      vòng dưới 1 giây cả trang web và TM1638 đều báo mất kết nối; bật lại
      RX, xác nhận trạng thái tự phục hồi về "OK" mà không cần khởi động
      lại TX

## Ràng buộc
- Chỉ sửa/thêm file trong `tx/main/telemetry.c`, `tx/main/telemetry.h`, và
  phần gọi trong `tx/main/app_main.c`, `tx/main/tm1638.c` (chỉ thêm lời gọi
  cập nhật LED, không đổi driver TM1638 đã có)
- Không đụng `rx/`, `common/protocol.h`, `knowledge.md`

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (mục 4.2, 4.3)
