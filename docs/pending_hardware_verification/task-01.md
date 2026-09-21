# Kế hoạch Kiểm chứng Phần cứng Sau: Task 01 (Khung Dự án & Giao thức Truyền thông)

> **Mục đích:** Ghi nhận các hạng mục kiểm chứng phần cứng còn chờ (`BLOCKED`) của Task 01 do tại thời điểm phát triển, phòng lab chưa cắm bo mạch ESP32-S3 thật qua cổng COM/UART theo chỉ đạo của người dùng.

---

## Danh mục kiểm chứng phần cứng còn chờ (`BLOCKED`)

- [ ] **1. Nạp và kiểm chứng boot log UART trên bo mạch TX thật**
  - **Điều kiện cần:**
    1. Bo mạch ESP32-S3 DevKitC (TX) cắm cáp USB vào máy tính qua cổng COM.
    2. Cài đặt tốc độ baud 115200 trên Serial Monitor.
  - **Thao tác kiểm chứng:**
    1. Chạy lệnh: `idf.py -C tx -p <COM_PORT> flash monitor`.
    2. Quan sát log khởi động in ra màn hình console.
    3. Xác nhận log có các dòng:
       - `sizeof(ctrl_packet_t) = 12`
       - `sizeof(telemetry_packet_t) = 8`
       - `CRC-16/CCITT-FALSE self-test: input="123456789", expected=0x29B1, actual=0x29B1 -> PASS`
  - **Bằng chứng cần lưu:** File log UART TX.

- [ ] **2. Nạp và kiểm chứng boot log UART trên bo mạch RX thật**
  - **Điều kiện cần:**
    1. Bo mạch ESP32-S3 DevKitC (RX) cắm cáp USB vào máy tính qua cổng COM.
    2. Cài đặt tốc độ baud 115200 trên Serial Monitor.
  - **Thao tác kiểm chứng:**
    1. Chạy lệnh: `idf.py -C rx -p <COM_PORT> flash monitor`.
    2. Quan sát log khởi động in ra màn hình console.
    3. Xác nhận log có các dòng:
       - `sizeof(ctrl_packet_t) = 12`
       - `sizeof(telemetry_packet_t) = 8`
       - `CRC-16/CCITT-FALSE self-test: input="123456789", expected=0x29B1, actual=0x29B1 -> PASS`
  - **Bằng chứng cần lưu:** File log UART RX.

- [ ] **3. Đo tần số nhấp nháy LED trạng thái GPIO2 trên cả 2 bo mạch**
  - **Điều kiện cần:**
    1. Cả 2 bo mạch TX và RX đã nạp firmware và đang được cấp nguồn hoạt động.
    2. Máy hiện sóng (Oscilloscope) hoặc quan sát trực quan bằng đồng hồ bấm giờ.
  - **Thao tác kiểm chứng:**
    1. Quan sát LED onboard nối với GPIO2.
    2. Đo chu kỳ sáng/tắt: 500 ms sáng, 500 ms tắt $\to$ chu kỳ đúng 1000 ms (tần số 1 Hz).
    3. Xác nhận firmware chạy ổn định trong ít nhất 60 giây không bị treo hay reset bất thường.
  - **Bằng chứng cần lưu:** Video hoặc dạng sóng đo được từ chân GPIO2.
