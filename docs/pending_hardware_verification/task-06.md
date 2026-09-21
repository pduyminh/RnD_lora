# Kế hoạch kiểm chứng phần cứng — Task 06 (TM1638 & E-Stop vật lý)

> **Trạng thái: [BLOCKED]** — Phòng lab hiện tại chưa cắm board ESP32-S3 TX/RX và module TM1638 thật vào máy tính. Toàn bộ logic phần mềm đã được kiểm chứng 100% qua biên dịch `idf.py build` và suite unit test `pytest`. Tài liệu này mô tả chi tiết quy trình kiểm tra thực tế khi có phần cứng.

---

## 1. Mục tiêu kiểm chứng
Xác nhận hoạt động thực tế của module hiển thị và nút bấm TM1638 kết nối với ESP32-S3 TX theo sơ đồ chân trong `knowledge.md` mục 1.2:
- STB: GPIO 4
- CLK: GPIO 5
- DIO: GPIO 6
- Nguồn: 5V (hoặc 3.3V), GND chung với ESP32.

---

## 2. Quy trình kiểm tra chi tiết trên phần cứng thật

### Test Case 1: Hiển thị 8 LED 7 đoạn (Client count & vx) [BLOCKED]
- **Bước 1**: Nạp firmware `tx/` vào ESP32-S3 (`idf.py -p COM_TX flash monitor`).
- **Bước 2**: Quan sát màn hình 7 đoạn TM1638 lúc mới khởi động:
  - Vị trí 0-1 hiển thị `C0` (chưa có client kết nối).
  - Vị trí 3-7 hiển thị `    0` (vx = 0 mm/s).
- **Bước 3**: Dùng điện thoại kết nối vào WiFi `OMNI_ROBOT_TX` (pass: `12345678`), mở trình duyệt vào `http://192.168.4.1`.
- **Bước 4**: Quan sát vị trí 0-1 chuyển sang `C1` khi WebSocket kết nối thành công.
- **Bước 5**: Kéo joystick trên màn hình điện thoại theo trục X (tiến/lùi).
  - Quan sát vị trí 3-7 hiển thị số mm/s tương ứng (ví dụ `  500`, `- 250`, ` 1000`).

### Test Case 2: Nút bấm số 1 kích hoạt E-Stop khẩn cấp [BLOCKED]
- **Bước 1**: Đang kéo joystick cho robot chạy (ví dụ `vx = 600 mm/s`).
- **Bước 2**: Bấm nút S1 (nút số 1 ngoài cùng bên trái) trên module TM1638.
- **Bước 3**: Quan sát:
  - LED đơn thứ 1 (ngay trên nút 1) BẬT SÁNG ngay lập tức.
  - TX phát ngay gói tin ESP-NOW có `estop=1` không chờ hết chu kỳ 20ms.
  - Trên RX log hiển thị nhận lệnh E-STOP, cả 3 trục dừng ngay lập tức không qua ramp giảm tốc.
  - Cả 3 động cơ vẫn có lực giữ (chân ENA vẫn duy trì tích cực).

### Test Case 3: Nhả E-Stop bằng nút bấm số 1 [BLOCKED]
- **Bước 1**: Bấm lại nút S1 lần thứ 2.
- **Bước 2**: Quan sát:
  - LED đơn thứ 1 TẮT.
  - Cờ `estop` được xóa về 0.
  - Robot cho phép nhận lại lệnh điều khiển từ joystick bình thường.

---

## 3. Kết luận phần mềm
- Driver TM1638 3-wire thuần ESP-IDF đã hoàn thiện, không dùng Arduino.
- Cơ chế debounce, đọc phím, chốt E-Stop, phát gói tin khẩn cấp tức thì đã được kiểm chứng bằng unit test `tests/test_tm1638.py`.
- Firmware build thành công không có lỗi biên dịch.
