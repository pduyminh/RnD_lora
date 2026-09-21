# Kế hoạch Kiểm chứng Phần cứng Sau: Task 04 (RX ESP-NOW Pipeline & Failsafe)

> **Mục đích:** Ghi nhận các hạng mục kiểm chứng phần cứng còn chờ (`BLOCKED`) của Task 04 do tại thời điểm phát triển, phòng lab chưa cắm 2 bo mạch ESP32-S3 (TX và RX) thật theo chỉ đạo của người dùng.

---

## Danh mục kiểm chứng phần cứng còn chờ (`BLOCKED`)

- [ ] **1. Kiểm chứng ghép nối ESP-NOW thực tế giữa 2 thiết bị ESP32 (TX và RX)**
  - **Điều kiện cần:**
    1. Bo mạch RX kết nối nguồn và driver động cơ, nạp firmware `rx/`.
    2. Bo mạch TX (hoặc thiết bị gửi test ESP-NOW thứ 2) nạp firmware phát gói `ctrl_packet_t` 50 Hz.
  - **Thao tác kiểm chứng:**
    1. Bật nguồn cả 2 thiết bị.
    2. Gửi gói tin điều khiển cố định (ví dụ $v_x = 0.5\text{ m/s}, v_y = 0, \omega = 0$).
    3. Quan sát qua UART RX: xác nhận gói nhận liên tục, CRC hợp lệ, seq tăng đều, số gói bị loại không tăng.
    4. Quan sát 3 trục động cơ: xác nhận quay đúng chiều và tốc độ theo mô hình động học kinematics T02.
  - **Bằng chứng cần lưu:** File log UART RX ghi nhận chuỗi nhận gói và trạng thái động cơ.

- [ ] **2. Kiểm chứng phản hồi gói tin Telemetry (`telemetry_packet_t`) về TX**
  - **Điều kiện cần:** TX chạy bộ thu callback ESP-NOW.
  - **Thao tác kiểm chứng:**
    1. Với mỗi gói `ctrl_packet_t` gửi đi, TX nhận lại gói `telemetry_packet_t` từ địa chỉ MAC của RX.
    2. Kiểm tra `seq_echo` khớp với `seq` vừa gửi, `link_ok = 1`, `magic = 0x5A`.
  - **Bằng chứng cần lưu:** Log UART phía TX xác nhận nhận gói telemetry echo.

- [ ] **3. Kiểm chứng cơ chế Dừng Khẩn Cấp (E-Stop phần mềm)**
  - **Điều kiện cần:** TX gửi gói tin có cờ `estop = 1` khi robot đang di chuyển ở tốc độ cao.
  - **Thao tác kiểm chứng:**
    1. Trong lúc động cơ đang quay nhanh, TX phát gói `estop = 1`.
    2. Quan sát động cơ dừng quay ngay lập tức (cắt xung trong vòng lặp hiện tại, không chờ chu trình giảm tốc ramp).
    3. Dùng tay vặn trục: xác nhận động cơ vẫn được khóa cứng vị trí nhờ chân ENA duy trì mức active LOW.
  - **Bằng chứng cần lưu:** Video ghi nhận động cơ dừng ngắt xung tức thì và giữ cứng trục.

- [ ] **4. Kiểm chứng Failsafe khi mất kết nối RF (> 300 ms)**
  - **Điều kiện cần:** TX đang gửi lệnh di chuyển liên tục, sau đó ngắt nguồn đột ngột (hoặc mang TX ra ngoài tầm sóng).
  - **Thao tác kiểm chứng:**
    1. Ngắt nguồn TX.
    2. Đồng hồ bấm giờ hoặc UART log RX: Sau 300 ms không nhận được gói tin, UART RX in dòng `FAILSAFE TRIGGERED`.
    3. Cả 3 trục động cơ giảm tốc mượt mà (smooth ramp / anti-jerk) về 0 trong khoảng vài trăm ms, không bị dừng khựng cơ khí đột ngột.
    4. Khi dừng hẳn, ENA vẫn được duy trì cấp lực giữ (khóa cứng trục xe, chống trôi dốc).
  - **Bằng chứng cần lưu:** Log UART in dòng `FAILSAFE TRIGGERED` và video quá trình giảm tốc êm ái.
