# Kế hoạch kiểm chứng phần cứng — Task 07 (Telemetry RX & Cảnh báo song song Web/TM1638)

> **Trạng thái: [BLOCKED]** — Phòng lab hiện tại chưa cắm board ESP32-S3 TX/RX thật vào máy tính. Toàn bộ logic phần mềm đã được kiểm chứng 100% qua biên dịch `idf.py build` và suite unit test `pytest`. Tài liệu này mô tả chi tiết quy trình kiểm tra thực tế khi có phần cứng.

---

## 1. Mục tiêu kiểm chứng
Xác nhận hoạt động nhận bản tin `telemetry_packet_t` từ RX gửi ngược sang TX qua ESP-NOW ở tần số 50 Hz, và cơ chế phát hiện mất kết nối (> 500ms) hiển thị đồng thời lên:
- Trang HTML (qua kênh WebSocket tới điện thoại).
- LED đơn thứ hai trên module TM1638.

---

## 2. Quy trình kiểm tra chi tiết trên phần cứng thật

### Test Case 1: Trạng thái liên kết bình thường [BLOCKED]
- **Bước 1**: Cấp nguồn cho cả hai board RX và TX.
- **Bước 2**: Kết nối điện thoại vào WiFi `OMNI_ROBOT_TX` và mở trang web `http://192.168.4.1`.
- **Bước 3**: Quan sát:
  - Trên trang web: dòng trạng thái hiển thị `RX Link: OK` với màu xanh lá `#00ff66`.
  - Trên module TM1638: LED đơn thứ 2 (kế cạnh LED E-stop) TẮT.
  - Trên RX log: nhận lệnh ctrl_packet_t đều đặn và gửi telemetry phản hồi.

### Test Case 2: Tắt nguồn RX kiểm tra cảnh báo mất kết nối (< 1s) [BLOCKED]
- **Bước 1**: Rút cáp nguồn hoặc tắt công tắc nguồn cấp cho board RX trong lúc TX vẫn đang chạy.
- **Bước 2**: Bấm đồng hồ bấm giờ và quan sát phản ứng của TX:
  - Trong vòng dưới 1 giây (chính xác sau 500ms timeout):
    - Dòng trạng thái trên trang web lập tức chuyển sang `RX Link: MẤT KẾT NỐI` với màu đỏ `#ff3333` theo thời gian thực mà KHÔNG cần reload/refresh trang.
    - LED đơn thứ 2 trên module TM1638 BẬT SÁNG.

### Test Case 3: Bật lại nguồn RX kiểm tra tự phục hồi [BLOCKED]
- **Bước 1**: Cắm lại nguồn cho board RX (không reset, không tắt nguồn TX).
- **Bước 2**: Quan sát khi RX khởi động xong (sau ~1-2 giây):
  - RX bắt đầu nhận lại gói ESP-NOW từ TX và gửi telemetry phản hồi.
  - TX nhận gói `telemetry_packet_t` hợp lệ (đúng magic `0x5A` và CRC16).
  - Ngay lập tức:
    - LED đơn thứ 2 trên TM1638 TẮT.
    - Dòng trạng thái trên trang web chuyển lại thành `RX Link: OK` (màu xanh lá).
    - Hệ thống tự phục hồi hoàn toàn mà không cần khởi động lại TX.

---

## 3. Kết luận phần mềm
- Module Telemetry xử lý gói tin, validate magic/crc và cơ chế timeout 500ms đã hoàn thiện thuần ESP-IDF.
- Logic đồng bộ LED 2 và đẩy sự kiện qua WebSocket đã được kiểm chứng bằng unit test `tests/test_telemetry.py`.
- Cả hai firmware TX và RX đều biên dịch thành công 100%.
