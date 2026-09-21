---
role: codex-advisor
task_id: T-2026-0919-01
round: 1
---

# Khuyến nghị thực thi

## Cách tiếp cận đề xuất
1. Trước khi tạo mã nguồn, báo blocker: `telemetry_packet_t` tại mục 4.2 chỉ có tổng kích thước packed là 7 bytes (`1+2+1+1+2`), mâu thuẫn với yêu cầu 8 bytes. Không tự ý thêm trường đệm hoặc bỏ `packed`; yêu cầu người dùng xác nhận sửa đặc tả trước khi triển khai.
2. Sau khi đặc tả được xác nhận, tạo đúng ba vùng `common/`, `tx/`, `rx/`; không thêm ESP-NOW, WiFi, web server, TM1638 hoặc điều khiển động cơ.
3. Tạo `common/protocol.h` chứa include guard, các kiểu số nguyên chuẩn, hai struct dùng `__attribute__((packed))`, khai báo hằng magic và hàm CRC-16/CCITT-FALSE. Nếu cần `common/protocol.c`, cấu hình `common/` thành ESP-IDF component dùng chung; ưu tiên triển khai `static inline` trong header để giữ cấu trúc tối giản.
4. Cài đặt CRC-16/CCITT-FALSE với tham số chuẩn: polynomial `0x1021`, initial value `0xFFFF`, không phản chiếu input/output, xor-out `0x0000`.
5. Dùng test vector chuẩn ASCII `"123456789"`; kết quả bắt buộc là `0x29B1`. Tại boot, cả TX và RX phải tính CRC thực tế rồi log input, expected và actual; không in một giá trị hard-code thay cho kết quả hàm.
6. Thêm `_Static_assert(sizeof(ctrl_packet_t) == 12, ...)` và `_Static_assert(sizeof(telemetry_packet_t) == kích_thước_đã_được_xác_nhận, ...)` để build thất bại ngay nếu layout sai.
7. Tạo project ESP-IDF độc lập cho `tx/` và `rx/`: mỗi project có `CMakeLists.txt`, `main/CMakeLists.txt`, `main/main.c`; cấu hình đường dẫn component/include dùng đường dẫn tương đối đến `common/`, không sao chép `protocol.h`.
8. Trong `app_main()` của cả hai firmware, log chính xác `sizeof(ctrl_packet_t) = 12` và kích thước telemetry đã chốt; chạy CRC self-test và log rõ `PASS` hoặc `FAIL`.
9. Cấu hình GPIO2 output và tạo vòng lặp FreeRTOS đảo trạng thái mỗi 500 ms, tạo chu kỳ nhấp nháy đầy đủ 1 Hz. Không dùng busy-wait.
10. Build riêng từ từng thư mục bằng `idf.py set-target esp32s3` rồi `idf.py build`; sau đó flash và monitor từng board để thu bằng chứng UART và quan sát LED.

## Ràng buộc bắt buộc AGY phải tuân thủ
- Chỉ được tạo/sửa: `common/protocol.h` và, nếu thật sự cần, `common/protocol.c`, `common/CMakeLists.txt`, `tx/CMakeLists.txt`, `tx/main/CMakeLists.txt`, `tx/main/main.c`, `rx/CMakeLists.txt`, `rx/main/CMakeLists.txt`, `rx/main/main.c`, `tests/test_protocol.py`, `docs/pending_hardware_verification/task-01.md`, các file nhật ký trong `tasks/T-2026-0919-01/`.
- CẤM sửa `task.md`, `.orchestrator/knowledge.md`, `lessons-learned.md` và mọi file ngoài phạm vi nêu trên.
- CẤM tự ý biến `telemetry_packet_t` thành 8 bytes bằng trường `reserved`, padding thủ công hoặc bỏ `packed` khi chưa có xác nhận cập nhật đặc tả.
- `ctrl_packet_t` phải giữ đúng thứ tự, kiểu và tên trường tại mục 4.1; kích thước packed phải là 12 bytes.
- `telemetry_packet_t` phải giữ đúng thứ tự, kiểu và tên trường tại mục 4.2 sau khi mâu thuẫn kích thước được giải quyết chính thức.
- CRC phải chỉ tính trên các byte đứng trước trường `crc16`; không được tính cả trường CRC.
- Không thêm dependency ngoài ESP-IDF chuẩn.
- Không triển khai ESP-NOW, WiFi, web server, TM1638, failsafe, động học hoặc điều khiển motor.
- GPIO trạng thái chỉ dùng GPIO2; không sử dụng các GPIO cấm 0, 3, 33–37, 45, 46.
- Không dùng cấu hình chỉ có tác dụng trên một máy cá nhân hoặc đường dẫn tuyệt đối.

## Rủi ro cần lưu ý
- Đặc tả telemetry tự mâu thuẫn: các trường packed chỉ chiếm 7 bytes nhưng acceptance yêu cầu 8 bytes → dừng triển khai phần này và xin xác nhận; không che lỗi bằng padding ngầm.
- Include `common/protocol.h` có thể hoạt động trong editor nhưng lỗi khi build độc lập → khai báo include/component path trong CMake của cả hai project và kiểm tra bằng hai build sạch.
- CRC cho kết quả sai do nhầm biến thể CCITT → khóa tham số thuật toán và kiểm tra `"123456789" == 0x29B1`.
- Test UART có thể luôn báo PASS do in hằng expected → log giá trị trả về trực tiếp từ `crc16_ccitt_false()` và so sánh runtime.
- LED có thể nhấp nháy 0,5 Hz nếu delay 1 giây sau mỗi lần đảo → đảo GPIO mỗi 500 ms để chu kỳ on/off đầy đủ là 1 giây.
- GPIO2 trên một số DevKitC không có LED onboard hoặc LED active-low → firmware vẫn phải toggle GPIO2; xác minh bằng LED thực tế hoặc oscilloscope/logic analyzer nếu board không gắn LED.
- Hai project có thể dùng build cache làm che lỗi → xóa riêng thư mục build của từng target hoặc chạy `idf.py fullclean` trước lần xác minh cuối.

## Tiêu chí để tự coi là "xong" (dùng cho self-check)
- Mâu thuẫn 7/8 bytes của `telemetry_packet_t` đã được người dùng giải quyết bằng một đặc tả rõ ràng; không còn giả định do AGY tự thêm.
- Chạy `cd tx; idf.py set-target esp32s3; idf.py fullclean; idf.py build` trả exit code 0.
- Chạy `cd rx; idf.py set-target esp32s3; idf.py fullclean; idf.py build` trả exit code 0.
- Compiler chấp nhận `_Static_assert(sizeof(ctrl_packet_t) == 12, ...)` và static assert telemetry theo kích thước đặc tả cuối cùng.
- Cả hai UART log đều chứa `sizeof(ctrl_packet_t) = 12` và dòng kích thước telemetry đúng với đặc tả đã chốt.
- Cả hai UART log đều chứa CRC test vector `"123456789"`, expected `0x29B1`, actual `0x29B1` và trạng thái `PASS`.
- Flash từng firmware bằng `idf.py -p <PORT> flash monitor` thành công, firmware không reset loop hoặc treo trong ít nhất 60 giây.
- GPIO2 trên cả hai board đổi mức mỗi `500 ± 50 ms`, tương ứng chu kỳ nhấp nháy `1,0 ± 0,1 Hz`, quan sát liên tục ít nhất 10 chu kỳ.
- Tìm kiếm mã nguồn không thấy API hoặc header triển khai ESP-NOW, WiFi, HTTP server, TM1638 hay motor trong `tx/`, `rx/`, `common/`.
- `git diff --name-only` chỉ liệt kê các file được phép tạo/sửa và không có `.orchestrator/knowledge.md`.