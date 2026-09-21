---
task_id: T-2026-0919-05
created_by: user
created_at: 2026-09-19
status: open
priority: medium
---

# Nhiệm vụ
Trên firmware `tx/`, dựng WiFi SoftAP + HTTP server (ESP-IDF `esp_http_server`)
phục vụ 1 trang HTML có joystick ảo 2 trục (điều khiển vx, vy) cộng 1 thanh
trượt hoặc joystick thứ hai cho omega, gửi qua WebSocket tới TX. TX đóng gói
thành `ctrl_packet_t` và gửi qua ESP-NOW sang RX ở đúng 50 Hz — kể cả khi
người dùng giữ nguyên joystick không đổi giá trị (không phải gửi khi có sự
kiện, để RX có nhịp đếm failsafe ổn định).

## Tiêu chí chấp nhận
- [ ] `idf.py build` thành công cho `tx/`
- [ ] TX khởi động SoftAP với SSID/password cấu hình được qua `menuconfig`
      hoặc hằng số đầu file (không hardcode rải rác)
- [ ] Truy cập được trang HTML qua trình duyệt khi kết nối vào AP của TX
- [ ] Trang HTML gửi giá trị joystick qua WebSocket, TX cập nhật biến
      (vx, vy, omega) nội bộ theo giá trị mới nhất nhận được
- [ ] Có 1 task/timer riêng gửi `ctrl_packet_t` qua ESP-NOW đúng chu kỳ
      20 ms ± 2 ms bất kể WebSocket có message mới hay không, `seq` tăng
      dần đúng 1 mỗi lần gửi, `crc16` tính đúng
- [ ] Khi không có client nào kết nối WebSocket, TX gửi gói với
      vx=vy=omega=0 (không gửi giá trị rác/giữ nguyên lệnh cuối vô thời hạn)
- [ ] Kiểm tra thủ công: kết nối điện thoại/laptop vào AP, mở trang, kéo
      joystick, xác nhận qua log UART của RX (từ T04) rằng gói nhận được
      liên tục và giá trị vx/vy/omega phản ánh đúng thao tác joystick

## Ràng buộc
- Chỉ sửa/thêm file trong `tx/main/` (web server, WebSocket handler,
  espnow sender, file HTML/JS nhúng hoặc SPIFFS)
- Dùng chung `common/protocol.h` đã có, không định nghĩa lại struct
- Không đụng `rx/`, không sửa `knowledge.md`
- Được phép thêm component ESP-IDF cần thiết cho web server/WebSocket nếu
  chưa có sẵn, ghi rõ lý do trong `plan.md`

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (mục 4.1, 4.3)
