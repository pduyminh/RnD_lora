---
name: knowledge
description: Kiến trúc & quy ước dự án robot 3 bánh omni (TX điều khiển - RX chấp hành) — đọc trước khi thực thi bất kỳ task nào
---

# Kiến trúc & quy ước dự án

> File này do người viết và chỉnh sửa thủ công. AI (Codex/AGY) chỉ đọc, không tự sửa.
> **CẦN BẠN SOÁT LẠI TRƯỚC KHI DÙNG:** các giá trị đánh dấu `[GIẢ ĐỊNH — CẦN ĐO THẬT]` là số tôi ước tính, phải xác nhận trên phần cứng thật rồi cập nhật lại đúng số đo được.

## 1. Phần cứng

### 1.1 Hai board
- **TX (điều khiển)**: ESP32-S3 DevKitC, chạy WiFi SoftAP + web server phục vụ trang HTML joystick, tích hợp module TM1638 (hiển thị + nút bấm), gửi lệnh điều khiển sang RX qua ESP-NOW ở 50 Hz (chu kỳ 20 ms).
- **RX (chấp hành)**: ESP32-S3 DevKitC, nhận lệnh ESP-NOW từ TX, tính động học tam giác, sinh xung PUL/DIR cho 3 driver ESD2505M-PZ điều khiển 3 động cơ YK257EC56E1 (closed-loop nội bộ qua encoder tích hợp, ESP32 không đọc encoder).

### 1.2 Sơ đồ chân

| Board | Tín hiệu | GPIO | Ghi chú |
|---|---|---|---|
| RX | Trục A — PUL | 4 | Bánh tại góc 0° |
| RX | Trục A — DIR | 5 | |
| RX | Trục B — PUL | 6 | Bánh tại góc 120° |
| RX | Trục B — DIR | 7 | |
| RX | Trục C — PUL | 15 | Bánh tại góc 240° |
| RX | Trục C — DIR | 16 | |
| RX | Trục A — ENA | 17 | Khóa giữ trục, active level xác nhận ở T03 |
| RX | Trục B — ENA | 18 | |
| RX | Trục C — ENA | 8 | |
| RX | LED trạng thái | 2 | Onboard LED |
| TX | TM1638 — STB | 4 | |
| TX | TM1638 — CLK | 5 | |
| TX | TM1638 — DIO | 6 | |
| TX | LED trạng thái | 2 | Onboard LED |

Không dùng GPIO0/3/45/46 (strapping) và GPIO33-37 (PSRAM) cho bất kỳ mục đích nào.

### 1.3 Đấu nối driver ESD2505M-PZ
- Ngõ vào PUL+/DIR+/ENA+ đấu chung dương **tại nguồn 5V riêng** (đã xác nhận), ESP32 kéo PUL-/DIR-/ENA- xuống mức thấp để tích cực (sinking vào GPIO).
- Điện trở hạn dòng cho mỗi đường quang cách ly (opto) tính theo common 5V: giả sử áp rơi LED opto ~1.2V, dòng mong muốn ~10 mA ⇒ R = (5 − 1.2) / 0.01 ≈ 380 Ω, dùng giá trị chuẩn **470 Ω** cho an toàn (dòng thực tế còn ~8 mA, vẫn đủ kích opto theo datasheet driver dòng Yako thông thường). **Giá trị này là ước tính, phải đối chiếu với dòng vào tối thiểu/tối đa của opto trong datasheet thật của ESD2505M-PZ ở task T03, không suy luận suông.**
- ESP32-S3 GPIO ở chế độ output bình thường (không cần open-drain) hoàn toàn sink được dòng ~8-10 mA mỗi chân một cách an toàn (giới hạn khuyến nghị mỗi chân <20 mA, tổng 9 đường PUL+DIR+ENA ~90 mA lúc đỉnh, trong ngưỡng cho phép của chip).
- Không dùng ngõ ALM (theo yêu cầu).
- DIP switch driver: vi bước 40 (8000 xung/vòng ÷ 200 bước/vòng động cơ 1.8°/bước = vi bước 40).
- Nguồn động lực (driver + motor) tách riêng khỏi nguồn ESP32, nhưng **bắt buộc nối chung GND** giữa hai nguồn.

### 1.3.1 Chân ENA — chính sách điều khiển
- Mỗi trục có 1 chân ENA riêng (xem bảng 1.2), điều khiển độc lập.
- **Mức tích cực (active level) của ENA CHƯA XÁC ĐỊNH** — driver có thể coi "kéo xuống thấp = enable" hoặc ngược lại tùy dòng sản phẩm Yako cụ thể. Phải đo/thử tại T03 và ghi kết quả vào bảng dưới đây.
- **Mặc định lúc boot**: cả 3 trục được enable (có lực giữ) ngay khi firmware khởi động xong, kể cả khi vx=vy=omega=0 — để robot không bị đẩy trôi khi đứng yên.
- **Khi E-stop (phần mềm, qua TM1638 hoặc web)**: chỉ dừng phát xung (PUL), **giữ nguyên ENA ở trạng thái enable** — không tắt lực giữ trục. Việc thả trục hoàn toàn (mất lực giữ) chỉ xảy ra khi rút nguồn động lực (E-stop phần cứng).

| Trục | Mức GPIO nào = enable (có lực giữ)? | Đã xác nhận? |
|---|---|---|
| A | `[CHƯA ĐO]` | ☐ |
| B | `[CHƯA ĐO]` | ☐ |
| C | `[CHƯA ĐO]` | ☐ |

### 1.4 Hình học robot
- Bố trí tam giác đều, 3 bánh omni tại góc 0°/120°/240° tính từ trục trước (trục X dương) của robot, ngược chiều kim đồng hồ nhìn từ trên xuống.
- Khoảng cách tâm robot đến bánh: L = 0.38 m
- Bán kính bánh: r = 0.05 m
- Chu vi bánh: 2πr ≈ 0.3142 m
- Độ phân giải: 8000 xung/vòng
- ⇒ Hệ số quy đổi: **1 m di chuyển bánh = 8000 / 0.3142 ≈ 25 465 xung**

### 1.5 Quy ước chiều quay (PHẢI XÁC NHẬN THỰC TẾ Ở TASK T03)
- Chiều dương của mỗi trục = chiều làm bánh đẩy robot ra phía trước theo phương pháp tuyến của bánh đó (tangent hướng ngược kim đồng hồ quanh tâm robot).
- Mức DIR pin nào ứng với chiều dương là điều **chưa biết cho đến khi test tay ở T03** — ghi kết quả đo được (HIGH hay LOW là chiều dương của từng trục) trở lại vào bảng dưới đây sau khi T03 hoàn tất:

| Trục | DIR=HIGH là chiều nào? | Đã xác nhận? |
|---|---|---|
| A | `[CHƯA ĐO]` | ☐ |
| B | `[CHƯA ĐO]` | ☐ |
| C | `[CHƯA ĐO]` | ☐ |

## 2. Động học (kinematics) — công thức chuẩn

Cho vận tốc robot-centric (vx, vy) đơn vị m/s và vận tốc góc ω (rad/s), góc bánh thứ i là θ_i ∈ {0°, 120°, 240°}:

```
v_wheel_i (m/s) = -sin(θ_i) * vx + cos(θ_i) * vy + L * ω
```

Đổi sang xung/giây:
```
pulses_per_sec_i = v_wheel_i * (8000 / (2 * π * r))
```

## 3. Giới hạn tốc độ

- Vận tốc tịnh tiến tối đa cho phép ở đầu vào lệnh: **vx, vy sao cho |v| ≤ 1.0 m/s**
- Vận tốc góc tối đa cho phép ở đầu vào lệnh: **ω ≤ 2.0 rad/s** `[GIẢ ĐỊNH — CẦN ĐO THẬT]`, đây chỉ là trần đầu vào, không phải giới hạn vật lý.
- `wheel_max_pps` (xung/giây tối đa 1 trục chịu được không mất bước, có tải): `[CHƯA ĐO — điền sau task T03]`
- Khi tổ hợp (vx, vy, ω) khiến bất kỳ bánh nào vượt `wheel_max_pps`: **co tỉ lệ cả 3 giá trị pulses_per_sec** theo cùng hệ số (giữ đúng hướng vector chuyển động, chỉ giảm độ lớn) cho đến khi bánh nhanh nhất vừa đúng `wheel_max_pps`.
- Gia tốc: ramp hình thang, gia tốc tối đa 2 m/s² quy đổi ra xung/giây² cho từng trục dựa theo hệ số ở mục 1.4.

## 4. Giao thức truyền thông (ESP-NOW, 50 Hz)

### 4.1 Gói lệnh TX → RX — `ctrl_packet_t` (12 bytes, packed)
```c
typedef struct __attribute__((packed)) {
    uint8_t  magic;         // = 0xA5, kiểm tra nhanh gói hợp lệ
    uint16_t seq;            // tăng dần mỗi gói, phát hiện mất gói/gói cũ
    int16_t  vx_mm_s;        // vận tốc trục X robot, mm/s
    int16_t  vy_mm_s;        // vận tốc trục Y robot, mm/s
    int16_t  omega_mrad_s;   // vận tốc góc, mrad/s (1000 = 1 rad/s)
    uint8_t  estop;          // 1 = dừng khẩn cấp ngay, bỏ qua mọi ramp
    uint16_t crc16;          // CRC-16/CCITT-FALSE tính trên các byte trước nó
} ctrl_packet_t;
```

### 4.2 Gói telemetry RX → TX — `telemetry_packet_t` (8 bytes, packed)
```c
typedef struct __attribute__((packed)) {
    uint8_t  magic;               // = 0x5A
    uint16_t seq_echo;             // seq của ctrl_packet_t gần nhất nhận được hợp lệ
    uint8_t  driver_fault_bitmap;  // dự phòng cho tương lai, hiện luôn = 0 (không đọc ALM)
    uint8_t  link_ok;              // 1 nếu chưa vượt failsafe timeout
    uint16_t crc16;
} telemetry_packet_t;
```

### 4.3 Failsafe
- RX coi kết nối mất nếu không nhận được `ctrl_packet_t` hợp lệ (đúng magic + CRC + seq mới hơn seq trước) trong 300 ms liên tục.
- Khi mất kết nối: RX ramp giảm tốc cả 3 trục về 0 theo cùng gia tốc tối đa ở mục 3, không dừng đột ngột.
- Khi nhận `estop = 1`: dừng ngay lập tức cả 3 trục, không ramp.

## 5. Ngôn ngữ/framework
- ESP-IDF thuần (không Arduino framework), build bằng `idf.py build` cho cả hai target `tx/` và `rx/`.
- Thư viện thêm mới cần nêu lý do trong `plan.md` của Codex trước khi AGY cài đặt (được phép thêm nếu cần thiết, ví dụ `mdns`, thư viện phục vụ web server tĩnh — nhưng ưu tiên dùng component có sẵn trong ESP-IDF trước).

## 6. Cấu trúc thư mục repo

```
repo/
├── common/
│   └── protocol.h          ← ctrl_packet_t, telemetry_packet_t, CRC16
├── tx/
│   ├── CMakeLists.txt
│   └── main/
├── rx/
│   ├── CMakeLists.txt
│   └── main/
└── .orchestrator/          ← theo multi-ai-orchestration-spec
```

## 7. An toàn
- E-stop phần cứng: rút jack nguồn động lực (không có công tắc cứng riêng).
- E-stop phần mềm: nút trên TM1638 (TX) gửi `estop=1` ngay lập tức, và/hoặc nút trên trang HTML.
