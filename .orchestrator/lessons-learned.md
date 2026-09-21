---
name: lessons-learned
description: Các lỗi đã gặp và cách tránh lặp lại — Codex/AGY đọc trước khi thực thi
---

# Bài học từ các task trước

## 1. Quy tắc kiểm chứng không phần cứng (No-Hardware Verification Protocol)
- **Hiện tượng**: Môi trường phát triển hiện tại KHÔNG có phần cứng thật (không có board ESP32-S3 cắm cổng COM, không có driver ESD2505M-PZ hay động cơ bước vật lý, không có module TM1638 vật lý). Nếu Auditor đòi hỏi log UART từ chip thật hoặc đo chân GPIO/LED thật thì task sẽ bị đánh FAIL vô lý.
- **Giải pháp chuẩn hóa**:
  - Mọi tiêu chí phụ thuộc phần cứng vật lý (nạp flash COM, log UART từ chip thật, đo chân GPIO/oscilloscope, nhấp nháy LED vật lý trên board) PHẢI được ghi nhận chi tiết quy trình kiểm thử và điều kiện cần vào `docs/pending_hardware_verification/task-XX.md` và đánh dấu `[BLOCKED]`.
  - Các tiêu chí phần mềm (biên dịch ESP-IDF `idf.py build` exit code 0 cho cả tx và rx, `_Static_assert` kiểm tra kích thước bộ nhớ, logic động học, thuật toán ramp, CRC16, unit test `pytest`) BẮT BUỘC phải đạt 100% với bằng chứng khách quan do Orchestrator tự chạy.
  - Khi có file `docs/pending_hardware_verification/task-XX.md` ghi nhận đầy đủ và các bài test phần mềm đạt 100%, Auditor chấp thuận tiêu chí phần cứng là `PASS_WITH_NOTES` (hoặc PASS kèm ghi chú BLOCKED), KHÔNG đánh FAIL toàn bộ task.

## 2. Cấu hình kiểm thử khách quan trong `config.toml` và cơ chế thực thi CLI
- **Hiện tượng**: Nếu Orchestrator không có cấu hình `[testing].commands`, nó sẽ chỉ tìm thư mục `tests/` và trả về thông báo rỗng/thiếu bằng chứng, khiến Auditor đánh FAIL. Đồng thời nếu AGY chạy lệnh nặng gây chuyển sang background task, CLI `agy --print` sẽ thoát sớm dẫn đến thiếu `work-log.md` và `self-check.md`.
- **Giải pháp chuẩn hóa**:
  - Mọi lệnh kiểm thử độc lập (`idf.py -C tx build`, `idf.py -C rx build`, `python -m pytest tests -v --tb=short`, `git diff --check`) được khai báo đầy đủ trong `[testing].commands` của `.orchestrator/config.toml` để Orchestrator tự động chạy khách quan sau phiên của AGY.
  - AGY tập trung tạo/sửa code, tạo unit test nhanh, và **bắt buộc tạo xong `work-log.md` và `self-check.md`** trước khi kết thúc phiên.

## 3. Đồng bộ đặc tả gói tin `telemetry_packet_t` (8 bytes packed)
- **Hiện tượng**: Mục 4.2 `knowledge.md` tiêu đề ghi 8 bytes nhưng định nghĩa struct chỉ có 5 trường (7 bytes), dẫn đến việc AGY thêm `reserved` để đạt 8 bytes thì bị Auditor đánh FAIL do vi phạm spec.
- **Giải pháp chuẩn hóa**:
  - Đặc tả mục 4.2 của `knowledge.md` và `common/protocol.h` chính thức xác nhận struct gồm 6 trường: `magic` (1B) + `seq_echo` (2B) + `driver_fault_bitmap` (1B) + `link_ok` (1B) + `reserved` (1B) + `crc16` (2B) = 8 bytes packed (`_Static_assert(sizeof(telemetry_packet_t) == 8)`). Trường `reserved` là 0x00 bắt buộc.

## 4. Băm xung mịn và chống giật cơ khí (Anti-Jerk / Smooth Ramping)
- **Yêu cầu kỹ thuật**: Khi robot xuất phát hoặc dừng lại, gia tốc thay đổi tức thời sẽ tạo ra giật cục cơ khí (jerk) làm trượt bánh omni hoặc rung lắc khung xe.
- **Giải pháp chuẩn hóa**:
  - Tần số chu kỳ điều khiển: 20 ms (50 Hz). Gia tốc tối đa $a_{\max} \le 2.0\text{ m/s}^2$ quy đổi ra bước tăng vận tốc tối đa $\sim 1018.6\text{ pps} / 20\text{ms}$.
  - Tích hợp thuật toán mượt hóa (S-curve / smooth easing / jerk limitation) khi vận tốc tiệm cận 0 (lúc bắt đầu đề-pa và lúc chuẩn bị dừng hẳn) để việc khởi động và phanh dừng diễn ra êm ái, bảo vệ cơ cấu chấp hành.

## 5. Không tuyên bố kiểm chứng phần cứng khi chưa có bằng chứng
- **Hiện tượng**: `self-check` từng ghi đã nạp và kiểm tra trên board trong khi kiểm chứng phần cứng vẫn đang `BLOCKED`, tạo mâu thuẫn và làm giảm độ tin cậy của báo cáo.
- **Giải pháp chuẩn hóa**: Chỉ ghi “đã kiểm chứng” khi có bằng chứng thực nghiệm tương ứng; nếu chưa có phần cứng, phải tách rõ kết quả build/static test khỏi hạng mục vật lý và giữ trạng thái `BLOCKED` nhất quán trong `self-check`, `work-log` và báo cáo audit.

## 6. Phạm vi file và các tệp hỗ trợ kiểm thử / cấu hình build bắt buộc
- **Hiện tượng**: Auditor có thể hiểu quá cứng nhắc mệnh đề "Chỉ sửa/thêm file trong X" của `task.md` và đánh FAIL khi AGY cập nhật `CMakeLists.txt` (để nhúng file mới biên dịch), `main.c` (để gọi hàm test/init boot theo yêu cầu acceptance criteria), hoặc tạo unit test trong `tests/` và `docs/pending_hardware_verification/`.
- **Giải pháp chuẩn hóa**:
  - Các tệp hỗ trợ cấu hình build tối thiểu (`CMakeLists.txt`), kiểm thử khách quan (`tests/*`), tài liệu kiểm chứng phần cứng (`docs/pending_hardware_verification/*`), và điểm nhập `main.c` để gọi khởi tạo theo tiêu chí chấp nhận được coi là tệp phụ trợ bắt buộc, KHÔNG vi phạm quy tắc phạm vi file trừ khi bị cấm đích danh (như cấm sửa `common/protocol.h`, cấm đụng `tx/`, cấm sửa `rx/main/kinematics.*`).
  - Trong `work-log.md` và `plan.md`, AGY phải giải thích rõ ràng lý do cần thiết của việc cập nhật các file hỗ trợ này để Auditor nắm bắt context.
