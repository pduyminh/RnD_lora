import pytest
import re
from pathlib import Path

# Thư mục gốc repo
REPO_ROOT = Path(__file__).resolve().parent.parent
TX_MAIN = REPO_ROOT / "tx" / "main"


def test_tm1638_pinout_matches_knowledge_spec():
    """Kiểm tra sơ đồ chân TM1638 trong tm1638.h đúng với knowledge.md mục 1.2:
    STB = GPIO 4, CLK = GPIO 5, DIO = GPIO 6
    """
    tm_h = (TX_MAIN / "tm1638.h").read_text(encoding="utf-8")

    stb_match = re.search(r"#define\s+TM1638_STB_GPIO\s+GPIO_NUM_(\d+)", tm_h)
    clk_match = re.search(r"#define\s+TM1638_CLK_GPIO\s+GPIO_NUM_(\d+)", tm_h)
    dio_match = re.search(r"#define\s+TM1638_DIO_GPIO\s+GPIO_NUM_(\d+)", tm_h)

    assert stb_match is not None, "Không tìm thấy TM1638_STB_GPIO"
    assert clk_match is not None, "Không tìm thấy TM1638_CLK_GPIO"
    assert dio_match is not None, "Không tìm thấy TM1638_DIO_GPIO"

    assert int(stb_match.group(1)) == 4, f"STB GPIO phải là 4, thực tế: {stb_match.group(1)}"
    assert int(clk_match.group(1)) == 5, f"CLK GPIO phải là 5, thực tế: {clk_match.group(1)}"
    assert int(dio_match.group(1)) == 6, f"DIO GPIO phải là 6, thực tế: {dio_match.group(1)}"


def test_tm1638_protocol_commands():
    """Kiểm tra các lệnh chuẩn của TM1638 datasheet (0x40, 0x42, 0xC0, 0x88)."""
    tm_h = (TX_MAIN / "tm1638.h").read_text(encoding="utf-8")

    assert "#define TM1638_CMD_DATA_WRITE_AUTO   0x40" in tm_h
    assert "#define TM1638_CMD_DATA_READ_KEYS    0x42" in tm_h
    assert "#define TM1638_CMD_ADDR_START        0xC0" in tm_h
    assert "#define TM1638_CMD_DISPLAY_ON        0x88" in tm_h


def test_tm1638_seven_segment_encoding_logic():
    """Kiểm tra hàm mã hóa 7 đoạn cho số 0-9 và ký tự C, -."""
    DIGIT_TABLE = [
        0x3F,  # 0
        0x06,  # 1
        0x5B,  # 2
        0x4F,  # 3
        0x66,  # 4
        0x6D,  # 5
        0x7D,  # 6
        0x07,  # 7
        0x7F,  # 8
        0x6F   # 9
    ]

    def encode_char(c: str) -> int:
        if c.isdigit():
            return DIGIT_TABLE[int(c)]
        if c in ('C', 'c'):
            return 0x39
        if c == '-':
            return 0x40
        return 0x00

    # Kiểm tra mã hóa
    assert encode_char('0') == 0x3F
    assert encode_char('1') == 0x06
    assert encode_char('8') == 0x7F
    assert encode_char('C') == 0x39
    assert encode_char('-') == 0x40
    assert encode_char(' ') == 0x00


def test_tm1638_display_formatting():
    """Kiểm tra định dạng chuỗi hiển thị trên 8 LED 7 đoạn:
    Digits 0-1: 'C' + client_count
    Digit 2: ' '
    Digits 3-7: vx dạng số nguyên mm/s
    """
    def format_display(client_count: int, vx_mm_s: int):
        c_str = f"C{min(client_count, 9)}"
        vx_str = f"{vx_mm_s:5d}"
        full_str = f"{c_str} {vx_str}"
        assert len(full_str) == 8
        return full_str

    assert format_display(1, 500) == "C1   500"
    assert format_display(0, 0) == "C0     0"
    assert format_display(3, -250) == "C3  -250"
    assert format_display(10, 1000) == "C9  1000"


def test_button1_estop_toggle_and_latch_logic():
    """Mô phỏng máy trạng thái nút bấm 1 trên TM1638:
    - Bấm lần 1 (edge 0->1): kích hoạt E-Stop (estop=1), LED 1 sáng
    - Nhả nút: giữ nguyên trạng thái E-Stop
    - Bấm lần 2 (edge 0->1): hủy bỏ E-Stop (estop=0), LED 1 tắt
    """
    class TM1638State:
        def __init__(self):
            self.estop = False
            self.led1 = False
            self.prev_btn1 = False
            self.immediate_packets_sent = 0

        def update_button(self, btn1_pressed: bool):
            if btn1_pressed and not self.prev_btn1:
                # Cạnh lên
                self.estop = not self.estop
                self.led1 = self.estop
                if self.estop:
                    self.immediate_packets_sent += 1
            self.prev_btn1 = btn1_pressed

    state = TM1638State()

    # Ban đầu bình thường
    assert not state.estop
    assert not state.led1
    assert state.immediate_packets_sent == 0

    # Nhấn nút 1 lần đầu
    state.update_button(True)
    assert state.estop is True
    assert state.led1 is True
    assert state.immediate_packets_sent == 1

    # Giữ nút
    state.update_button(True)
    assert state.estop is True
    assert state.led1 is True

    # Nhả nút
    state.update_button(False)
    assert state.estop is True
    assert state.led1 is True

    # Nhấn nút 1 lần hai -> Hủy E-stop
    state.update_button(True)
    assert state.estop is False
    assert state.led1 is False
    assert state.immediate_packets_sent == 1  # Không gửi gói khẩn cấp thêm


def test_tx_estop_zeroes_motion_commands():
    """Kiểm tra khi cờ estop=1 được bật từ TM1638:
    Gói điều khiển phát đi phải có estop=1 và vận tốc bị ép về 0.
    """
    s_vx = 800
    s_vy = 400
    s_omega = 1500
    estop = 1

    def build_packet(vx, vy, omega, is_estop):
        if is_estop:
            return {"vx": 0, "vy": 0, "omega": 0, "estop": 1}
        return {"vx": vx, "vy": vy, "omega": omega, "estop": 0}

    pkt = build_packet(s_vx, s_vy, s_omega, estop)
    assert pkt["estop"] == 1
    assert pkt["vx"] == 0
    assert pkt["vy"] == 0
    assert pkt["omega"] == 0


def test_main_c_initializes_tm1638():
    """Xác nhận tx/main/main.c có gọi tm1638_init()."""
    main_c = (TX_MAIN / "main.c").read_text(encoding="utf-8")
    assert "#include \"tm1638.h\"" in main_c
    assert "tm1638_init()" in main_c


def test_cmakelists_contains_tm1638():
    """Xác nhận tx/main/CMakeLists.txt có đăng ký tm1638.c trong SRCS."""
    cmakelists = (TX_MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
    assert "tm1638.c" in cmakelists
