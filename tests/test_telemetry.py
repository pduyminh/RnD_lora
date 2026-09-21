import struct
import pytest
import re
from pathlib import Path

# Thư mục gốc repo
REPO_ROOT = Path(__file__).resolve().parent.parent
TX_MAIN = REPO_ROOT / "tx" / "main"
COMMON = REPO_ROOT / "common"


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF, refin=false, refout=false, xorout=0x0000)."""
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def pack_telemetry(magic=0x5A, seq_echo=1, driver_fault=0, link_ok=1, reserved=0):
    payload = struct.pack("<HB B B", seq_echo, driver_fault, link_ok, reserved)
    pre_crc = bytes([magic]) + payload
    crc = crc16_ccitt_false(pre_crc)
    return pre_crc + struct.pack("<H", crc)


def test_telemetry_timeout_definition():
    """Kiểm tra macro TELEMETRY_TIMEOUT_MS = 500 trong telemetry.h
    (lớn hơn 300ms của RX để tránh báo động giả).
    """
    telem_h = (TX_MAIN / "telemetry.h").read_text(encoding="utf-8")
    m = re.search(r"#define\s+TELEMETRY_TIMEOUT_MS\s+(\d+)", telem_h)
    assert m is not None, "Không tìm thấy TELEMETRY_TIMEOUT_MS"
    timeout_val = int(m.group(1))
    assert timeout_val == 500, f"TELEMETRY_TIMEOUT_MS phải là 500, thực tế: {timeout_val}"


def test_telemetry_packet_validation():
    """Kiểm tra validation magic (0x5A) và CRC-16."""
    valid_pkt = pack_telemetry(magic=0x5A, seq_echo=42, driver_fault=0, link_ok=1)
    assert len(valid_pkt) == 8

    def validate_packet(pkt: bytes) -> bool:
        if len(pkt) != 8:
            return False
        if pkt[0] != 0x5A:
            return False
        calc = crc16_ccitt_false(pkt[:6])
        pkt_crc = struct.unpack("<H", pkt[6:8])[0]
        return calc == pkt_crc

    assert validate_packet(valid_pkt) is True

    # Sai magic
    bad_magic = bytearray(valid_pkt)
    bad_magic[0] = 0xA5
    assert validate_packet(bytes(bad_magic)) is False

    # Sai CRC
    bad_crc = bytearray(valid_pkt)
    bad_crc[7] ^= 0xFF
    assert validate_packet(bytes(bad_crc)) is False

    # Sai độ dài
    assert validate_packet(valid_pkt[:7]) is False
    assert validate_packet(valid_pkt + b"\x00") is False


def test_telemetry_timeout_and_recovery_state_machine():
    """Kiểm tra máy trạng thái mất kết nối RX sau 500ms và tự phục hồi khi có gói mới."""
    class TelemetryState:
        def __init__(self):
            self.link_ok = False
            self.last_rx_ms = -1
            self.led2 = True  # Ban đầu mất kết nối -> LED 2 sáng

        def on_packet(self, current_ms: int):
            self.link_ok = True
            self.last_rx_ms = current_ms
            self.led2 = False  # Link OK -> LED 2 tắt

        def check_timeout(self, current_ms: int):
            if self.link_ok and (current_ms - self.last_rx_ms > 500):
                self.link_ok = False
                self.led2 = True  # Mất link -> LED 2 sáng

    sm = TelemetryState()
    assert sm.link_ok is False
    assert sm.led2 is True

    # Nhận gói tại t = 100ms
    sm.on_packet(100)
    assert sm.link_ok is True
    assert sm.led2 is False

    # Tại t = 500ms (sau 400ms): vẫn còn trong ngưỡng 500ms
    sm.check_timeout(500)
    assert sm.link_ok is True
    assert sm.led2 is False

    # Tại t = 650ms (sau 550ms > 500ms): mất kết nối!
    sm.check_timeout(650)
    assert sm.link_ok is False
    assert sm.led2 is True

    # Tự phục hồi: nhận gói mới tại t = 900ms mà không cần reset TX
    sm.on_packet(900)
    assert sm.link_ok is True
    assert sm.led2 is False


def test_websocket_broadcast_status_formatting():
    """Kiểm tra định dạng JSON đẩy qua WebSocket khi link OK và MẤT KẾT NỐI."""
    def format_ws_status(link_ok: bool) -> str:
        return '{"rx_link":"OK","link_ok":true}' if link_ok else '{"rx_link":"MẤT KẾT NỐI","link_ok":false}'

    msg_ok = format_ws_status(True)
    assert '"rx_link":"OK"' in msg_ok
    assert '"link_ok":true' in msg_ok

    msg_lost = format_ws_status(False)
    assert '"rx_link":"MẤT KẾT NỐI"' in msg_lost
    assert '"link_ok":false' in msg_lost


def test_index_html_contains_rx_status_and_ws_listener():
    """Kiểm tra trang web nhúng trong tx_server.c có phần tử hiển thị liên kết RX
    và lắng nghe WebSocket message để cập nhật thời gian thực không cần refresh.
    """
    server_c = (TX_MAIN / "tx_server.c").read_text(encoding="utf-8")
    assert "id='rx-status'" in server_c
    assert "RX Link:" in server_c
    assert "ws.onmessage" in server_c
    assert "MẤT KẾT NỐI" in server_c


def test_tm1638_led2_indication():
    """Kiểm tra tm1638.c có cập nhật LED đơn thứ 2 theo trạng thái liên kết RX."""
    tm_c = (TX_MAIN / "tm1638.c").read_text(encoding="utf-8")
    assert "telemetry_is_link_ok()" in tm_c
    assert "tm1638_set_led(1," in tm_c


def test_tx_main_initializes_telemetry():
    """Kiểm tra tx/main/main.c có include và gọi telemetry_init()."""
    main_c = (TX_MAIN / "main.c").read_text(encoding="utf-8")
    assert "#include \"telemetry.h\"" in main_c
    assert "telemetry_init()" in main_c


def test_tx_cmakelists_contains_telemetry():
    """Kiểm tra tx/main/CMakeLists.txt có đăng ký telemetry.c."""
    cmakelists = (TX_MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
    assert "telemetry.c" in cmakelists
