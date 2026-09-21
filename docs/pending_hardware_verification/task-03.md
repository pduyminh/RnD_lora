# Kế hoạch Kiểm chứng Phần cứng Sau: Task 03 (RX MCPWM Pulse & ENA Axis Driver)

> **Mục đích:** Ghi nhận các hạng mục kiểm chứng phần cứng còn chờ (`BLOCKED`) của Task 03 do tại thời điểm phát triển, phòng lab chưa cắm bo mạch ESP32-S3 và cụm driver ESD2505M-PZ + động cơ YK257EC56E1 thật theo chỉ đạo của người dùng.

---

## Danh mục kiểm chứng phần cứng còn chờ (`BLOCKED`)

- [ ] **1. Đấu nối driver ESD2505M-PZ và động cơ bước YK257EC56E1 thật**
  - **Điều kiện cần:**
    1. Bo mạch ESP32-S3 RX kết nối qua cổng COM.
    2. Cụm driver ESD2505M-PZ, nguồn 24V-48V DC, động cơ hybrid stepper YK257EC56E1.
    3. Trở hạn dòng 470 $\Omega$ trên đường cathode PUL/DIR/ENA sinking 5V common-anode.
  - **Thao tác kiểm chứng:**
    1. Nạp firmware RX: `idf.py -C rx -p <COM_PORT> flash monitor`.
    2. Kiểm tra mức tín hiệu GPIO: 3.3V logic kéo mức LOW ổn định optocoupler driver ESD2505M-PZ.
    3. Đo dòng qua optocoupler nằm trong khoảng 8 mA - 15 mA chuẩn của driver.
  - **Bằng chứng cần lưu:** Ghi nhận đồng hồ vạn năng hoặc oscilloscope đo xung.

- [ ] **2. Kiểm chứng tốc độ quay thực tế với tần số 8000 PPS (1 vòng/giây)**
  - **Điều kiện cần:** Driver cài đặt DIP switch 8000 xung/vòng (microstepping 40x).
  - **Thao tác kiểm chứng:**
    1. Phát xung 8000 PPS cho từng trục A, B, C.
    2. Đếm tốc độ quay trục động cơ bằng đồng hồ bấm giờ hoặc tachometer quang học.
    3. Xác nhận tốc độ đúng 60 RPM (1.0 vòng/giây).
  - **Bằng chứng cần lưu:** Video ghi nhận chuyển động quay hoặc oscilloscope đo tần số xung chân PUL (8 kHz vuông, 50% duty).

- [ ] **3. Xác định chiều quay thực tế ứng với DIR=HIGH cho 3 trục**
  - **Điều kiện cần:** Động cơ đã lắp lên khung robot omni tam giác.
  - **Thao tác kiểm chứng:**
    1. Kích hoạt DIR=HIGH, kiểm tra chiều quay trục A, B, C nhìn từ ngoài vào bánh xe (theo chiều kim đồng hồ hay ngược chiều kim đồng hồ).
    2. Điền kết quả thực tế vào bảng "Quy ước chiều quay" mục 1.5 của `knowledge.md`.
  - **Bằng chứng cần lưu:** Nhật ký đo chiều quay thực tế.

- [ ] **4. Đo tốc độ tối đa không mất bước (`wheel_max_pps`) dưới tải**
  - **Điều kiện cần:** Robot lắp tải thực tế hoặc tỳ tay mô phỏng ma sát sàn.
  - **Thao tác kiểm chứng:**
    1. Tăng dần PPS từ 10000 đến 60000 pps qua bộ gia tốc Anti-Jerk.
    2. Chạy liên tục ít nhất 30 giây ở mỗi mức tốc độ.
    3. Xác nhận động cơ không bị rít, không trượt bước (stall).
    4. Ghi nhận giá trị trần vào mục 3 của `knowledge.md`.
  - **Bằng chứng cần lưu:** Bảng số liệu test tải liên tục 30s.

- [ ] **5. Kiểm tra cơ chế lực giữ ENA (Holding Torque) và dừng khẩn cấp (E-Stop)**
  - **Điều kiện cần:** Firmware đã khởi động (`axis_driver_init` hoàn tất).
  - **Thao tác kiểm chứng:**
    1. Khi vừa boot (chưa có lệnh di chuyển): Dùng tay cố vặn trục động cơ, xác nhận trục bị khóa cứng bởi dòng giữ (ENA active LOW).
    2. Gọi `axis_set_enable(axis, false)`: Xác nhận trục quay nhẹ bằng tay (thả tự do).
    3. Khi đang quay ở tốc độ cao, gọi `axis_stop_all()`: Xác nhận xung PUL cắt tức thì (động cơ dừng chuyển động ngay lập tức), nhưng trục vẫn bị khóa cứng giữ vị trí (không bị trôi trượt do quán tính).
  - **Bằng chứng cần lưu:** Video thử nghiệm lực giữ bằng tay.
