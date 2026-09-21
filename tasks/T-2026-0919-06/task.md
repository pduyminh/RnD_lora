---
task_id: T-2026-0919-06
created_by: user
created_at: 2026-09-19
status: open
priority: medium
---

# Nhiệm vụ
Tích hợp module TM1638 (8 LED 7 đoạn + 8 LED đơn + 8 nút bấm, giao tiếp
3 dây STB/CLK/DIO) vào firmware `tx/` theo sơ đồ chân mục 1.2 trong
`knowledge.md`. Dùng để hiển thị trạng thái kết nối/tốc độ hiện tại và làm
nút E-stop phần mềm vật lý, độc lập với trang web (phòng khi mất kết nối
WiFi từ điện thoại vẫn bấm được nút dừng khẩn cấp tại chỗ).

## Tiêu chí chấp nhận
- [ ] `idf.py build` thành công cho `tx/`
- [ ] Driver TM1638 tự viết hoặc thêm component, giao tiếp đúng giao thức
      3 dây, không dùng thư viện Arduino (dự án dùng ESP-IDF thuần)
- [ ] Bấm nút số 1 trên TM1638 → đặt cờ `estop=1` gửi ngay trong gói
      ESP-NOW kế tiếp (không đợi đủ chu kỳ 20ms nếu có thể gửi sớm hơn),
      giữ trạng thái estop cho đến khi bấm lại nút đó để bỏ estop
- [ ] Hiển thị lên 8 LED 7 đoạn: tối thiểu trạng thái AP (số client đang
      kết nối) và giá trị vx hiện tại (dạng số nguyên mm/s)
- [ ] LED đơn thứ nhất sáng khi đang ở trạng thái estop, tắt khi bình thường
- [ ] Kiểm tra thủ công: bấm nút estop trong lúc joystick đang ra lệnh di
      chuyển, xác nhận RX (qua log T04) dừng ngay lập tức không ramp

## Ràng buộc
- Chỉ sửa/thêm file trong `tx/main/tm1638.c`, `tx/main/tm1638.h`, và phần
  gọi trong `tx/main/app_main.c` — không sửa lại web server/WebSocket đã
  làm ở T05
- Không đụng `rx/`, `common/protocol.h`, `knowledge.md`

## Ngữ cảnh liên quan
- Xem thêm: .orchestrator/knowledge.md (mục 1.2, 7)
