# Kế hoạch Kiểm chứng Phần cứng Sau: Task 05 (TX SoftAP, Web Virtual Joystick & 50Hz ESP-NOW)

> **Mục đích:** Ghi nhận các hạng mục kiểm chứng phần cứng còn chờ (`BLOCKED`) của Task 05 do tại thời điểm phát triển, phòng lab chưa cắm bo mạch ESP32-S3 TX thật theo chỉ đạo của người dùng.

---

## Danh mục kiểm chứng phần cứng còn chờ (`BLOCKED`)

- [ ] **1. Kiểm chứng phát WiFi SoftAP và kết nối từ điện thoại/laptop**
  - **Điều kiện cần:**
    1. Bo mạch ESP32-S3 TX cắm nguồn hoặc USB.
    2. Thiết bị di động (smartphone hoặc laptop) có WiFi.
  - **Thao tác kiểm chứng:**
    1. Quét mạng WiFi: tìm thấy SSID `OMNI_ROBOT_TX`.
    2. Nhập mật khẩu `12345678` và kết nối thành công, nhận địa chỉ IP qua DHCP (192.168.4.x).
    3. Mở trình duyệt web truy cập `http://192.168.4.1`: giao diện điều khiển Omni Robot tải lên hoàn chỉnh với Virtual Joystick, thanh trượt Omega và nút Emergency Stop.
  - **Bằng chứng cần lưu:** Ảnh chụp màn hình điện thoại kết nối AP và hiển thị giao diện web.

- [ ] **2. Kiểm chứng WebSocket và truyền dữ liệu thời gian thực**
  - **Điều kiện cần:** Trình duyệt đã kết nối WebSocket tới `ws://192.168.4.1/ws`.
  - **Thao tác kiểm chứng:**
    1. Quan sát dòng trạng thái trên trang web: chuyển sang màu xanh "WebSocket: Connected".
    2. Kéo joystick trên màn hình cảm ứng: các giá trị vx, vy thay đổi tương ứng.
    3. Kéo thanh trượt Omega: giá trị omega thay đổi từ -2000 đến 2000 mrad/s.
    4. Thả tay khỏi màn hình: cần điều khiển tự động bật về tâm (vx=0, vy=0), thanh trượt về 0.
  - **Bằng chứng cần lưu:** Video thao tác kéo thả joystick trên điện thoại.

- [ ] **3. Kiểm chứng tần số phát ESP-NOW 50 Hz trên bo mạch TX**
  - **Điều kiện cần:** Dùng máy hiện sóng (oscilloscope) hoặc thiết bị bắt gói RF/UART log.
  - **Thao tác kiểm chứng:**
    1. Đo chu kỳ phát gói tin của TX: chu kỳ gửi đều đặn 20 ms ± 2 ms (tần số 50 Hz).
    2. Khi ngắt kết nối trình duyệt: TX vẫn phát đều đặn 50 Hz với vx=0, vy=0, omega=0 (không ngắt nhịp đếm của RX).
  - **Bằng chứng cần lưu:** Log UART ghi nhận chu kỳ phát 20ms hoặc ảnh chụp máy hiện sóng.

- [ ] **4. Kiểm chứng nút Emergency Stop trên giao diện Web**
  - **Điều kiện cần:** Robot đang nhận lệnh chạy liên tục.
  - **Thao tác kiểm chứng:**
    1. Bấm nút đỏ "EMERGENCY STOP" trên web.
    2. TX phát gói tin có cờ `estop = 1`.
    3. RX lập tức dừng ngắt xung 3 trục và giữ nguyên lực giữ ENA.
  - **Bằng chứng cần lưu:** Log UART xác nhận cờ estop được phát đi.
