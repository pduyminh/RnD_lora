# Kế hoạch Kiểm chứng Phần cứng Sau: Task 02 (Động học Tam giác & Anti-Jerk Ramp)

> **Mục đích:** Ghi nhận các hạng mục kiểm chứng phần cứng còn chờ (`BLOCKED`) của Task 02 do tại thời điểm phát triển, phòng lab chưa cắm bo mạch ESP32-S3 thật qua cổng COM/UART theo chỉ đạo của người dùng.

---

## Danh mục kiểm chứng phần cứng còn chờ (`BLOCKED`)

- [ ] **1. Nạp firmware RX lên bo mạch thật và kiểm chứng log UART cho 3 bộ vector test**
  - **Điều kiện cần:**
    1. Bo mạch ESP32-S3 DevKitC (RX) cắm cáp USB vào máy tính qua cổng COM.
    2. Cài đặt tốc độ baud 115200 trên Serial Monitor.
  - **Thao tác kiểm chứng:**
    1. Chạy lệnh: `idf.py -C rx -p <COM_PORT> flash monitor`.
    2. Quan sát log in ra màn hình console và đối chiếu với bảng số liệu tính toán chuẩn:
       - **Bộ test 1** ($v_x=1.0, v_y=0.0, \omega=0.0$):
         `Kinematics Test 1 (vx=1.0, vy=0.0, w=0.0) -> Axis A: 0, Axis B: -22053, Axis C: 22053 pps`
       - **Bộ test 2** ($v_x=0.0, v_y=0.0, \omega=2.0$):
         `Kinematics Test 2 (vx=0.0, vy=0.0, w=2.0) -> Axis A: 19353, Axis B: 19353, Axis C: 19353 pps`
       - **Bộ test 3** ($v_x=0.5, v_y=0.5, \omega=1.0$):
         `Kinematics Test 3 (vx=0.5, vy=0.5, w=1.0) -> Axis A: 22409, Axis B: -7716, Axis C: 14337 pps`
       - **Ramp bước đầu tiên (Anti-Jerk)**:
         `Kinematics Ramp 1-step (20ms) -> Axis A: 0, Axis B: -255, Axis C: 255 pps (Anti-Jerk start)`
  - **Bằng chứng cần lưu:** File log capture từ UART RX.

- [ ] **2. Kiểm tra độ ổn định runtime và LED trạng thái GPIO2**
  - **Điều kiện cần:** Bo mạch RX hoạt động liên tục trong tối thiểu 60 giây.
  - **Thao tác kiểm chứng:**
    1. Quan sát LED onboard GPIO2 nhấp nháy chu kỳ 1 Hz (500 ms ON / 500 ms OFF).
    2. Xác nhận không có ngoại lệ CPU (Guru Meditation), Watchdog reset hay tràn ngăn xếp.
  - **Bằng chứng cần lưu:** Video hoặc ghi nhận trạng thái LED.
