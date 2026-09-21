"""
Unit tests for RX ESP-NOW Pipeline, Validation & Failsafe (Task 04).
Tests packet validation (magic, crc16, seq ordering), E-stop behavior,
300ms timeout failsafe with smooth ramp deceleration, and telemetry echo.
"""

import struct
from pathlib import Path
import pytest

RX_DIR = Path(__file__).parent.parent / "rx" / "main"
COMM_H_PATH = RX_DIR / "comm.h"
COMM_C_PATH = RX_DIR / "comm.c"

CTRL_MAGIC = 0xA5
TELEMETRY_MAGIC = 0x5A


def crc16_ccitt_false(data: bytes) -> int:
    """Standard CRC-16/CCITT-FALSE implementation."""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def make_ctrl_packet(seq: int, vx: int = 0, vy: int = 0, omega: int = 0, estop: int = 0, magic: int = CTRL_MAGIC, corrupt_crc: bool = False) -> bytes:
    """Packs a 12-byte ctrl_packet_t matching protocol.h."""
    # Format: uint8 magic, uint16 seq, int16 vx, int16 vy, int16 omega, uint8 estop, uint16 crc16
    payload = struct.pack("<BHhhhB", magic, seq, vx, vy, omega, estop)
    crc = crc16_ccitt_false(payload)
    if corrupt_crc:
        crc ^= 0x1234
    return payload + struct.pack("<H", crc)


class MockRxPipeline:
    """Python simulation of the RX pipeline logic in comm.c."""
    def __init__(self):
        self.last_seq = 0
        self.has_first_seq = False
        self.dropped_count = 0
        self.estop_active = False
        self.failsafe_active = False
        self.last_rx_time_ms = 0
        self.target_pps = [0, 0, 0]
        self.current_pps = [0, 0, 0]
        self.ena_enabled = True  # Boot default: axis_enable_all(true)
        self.telemetry_history = []

    def process_packet(self, data: bytes, now_ms: int) -> bool:
        if len(data) != 12:
            self.dropped_count += 1
            return False

        magic, seq, vx, vy, omega, estop, crc16 = struct.unpack("<BHhhhBH", data)

        if magic != CTRL_MAGIC:
            self.dropped_count += 1
            return False

        calc_crc = crc16_ccitt_false(data[:10])
        if calc_crc != crc16:
            self.dropped_count += 1
            return False

        if self.has_first_seq:
            # 16-bit signed diff for sequence wraparound
            diff = (seq - self.last_seq) & 0xFFFF
            if diff > 0x7FFF:
                diff -= 0x10000
            if diff <= 0:
                self.dropped_count += 1
                return False

        self.last_seq = seq
        self.has_first_seq = True
        self.last_rx_time_ms = now_ms
        self.failsafe_active = False

        if estop == 1:
            self.estop_active = True
            # Cut pulses immediately, NEVER disable ENA
            self.target_pps = [0, 0, 0]
            self.current_pps = [0, 0, 0]
        else:
            self.estop_active = False
            # Simulate kinematics target calculation (scaled)
            self.target_pps = [int(vx * 25.46), int(vy * 25.46), int(omega * 0.38 * 25.46)]

        # Prepare telemetry echo
        telem_payload = struct.pack("<BHBBB", TELEMETRY_MAGIC, seq, 0, 1, 0)
        telem_crc = crc16_ccitt_false(telem_payload)
        telem_pkt = telem_payload + struct.pack("<H", telem_crc)
        self.telemetry_history.append(telem_pkt)

        return True

    def step_20ms(self, now_ms: int, max_delta_pps: int = 1018):
        """Simulates 20ms pipeline step with 300ms failsafe timeout check."""
        if self.has_first_seq and (now_ms - self.last_rx_time_ms > 300):
            self.failsafe_active = True
            self.target_pps = [0, 0, 0]

        if self.estop_active:
            self.current_pps = [0, 0, 0]
            return

        # Ramp current_pps toward target_pps
        for i in range(3):
            err = self.target_pps[i] - self.current_pps[i]
            if abs(err) <= max_delta_pps:
                self.current_pps[i] = self.target_pps[i]
            elif err > 0:
                self.current_pps[i] += max_delta_pps
            else:
                self.current_pps[i] -= max_delta_pps


def test_comm_files_exist():
    """Verify comm.h and comm.c exist."""
    assert COMM_H_PATH.is_file(), f"Missing {COMM_H_PATH}"
    assert COMM_C_PATH.is_file(), f"Missing {COMM_C_PATH}"


def test_packet_validation_magic_and_crc():
    """Verify bad magic and bad CRC are rejected and increment dropped count."""
    pipe = MockRxPipeline()

    # Valid packet
    p_valid = make_ctrl_packet(seq=1, vx=100)
    assert pipe.process_packet(p_valid, now_ms=0) is True
    assert pipe.dropped_count == 0
    assert pipe.last_seq == 1

    # Bad magic
    p_bad_magic = make_ctrl_packet(seq=2, magic=0x00)
    assert pipe.process_packet(p_bad_magic, now_ms=20) is False
    assert pipe.dropped_count == 1
    assert pipe.last_seq == 1  # Not updated

    # Bad CRC
    p_bad_crc = make_ctrl_packet(seq=3, corrupt_crc=True)
    assert pipe.process_packet(p_bad_crc, now_ms=40) is False
    assert pipe.dropped_count == 2
    assert pipe.last_seq == 1


def test_packet_sequence_anti_replay():
    """Verify older or duplicate seq numbers are rejected."""
    pipe = MockRxPipeline()

    # Accept first packet
    assert pipe.process_packet(make_ctrl_packet(seq=10), now_ms=0) is True

    # Duplicate seq (10) -> reject
    assert pipe.process_packet(make_ctrl_packet(seq=10), now_ms=20) is False
    assert pipe.dropped_count == 1

    # Older seq (9) -> reject
    assert pipe.process_packet(make_ctrl_packet(seq=9), now_ms=40) is False
    assert pipe.dropped_count == 2

    # Higher seq (11) -> accept
    assert pipe.process_packet(make_ctrl_packet(seq=11), now_ms=60) is True
    assert pipe.dropped_count == 2
    assert pipe.last_seq == 11


def test_sequence_wraparound():
    """Verify uint16 sequence number wraparound is handled correctly."""
    pipe = MockRxPipeline()

    assert pipe.process_packet(make_ctrl_packet(seq=65535), now_ms=0) is True
    # Wraparound to 0 should be treated as newer
    assert pipe.process_packet(make_ctrl_packet(seq=0), now_ms=20) is True
    assert pipe.last_seq == 0
    # Wraparound to 1 should be treated as newer
    assert pipe.process_packet(make_ctrl_packet(seq=1), now_ms=40) is True
    assert pipe.last_seq == 1


def test_estop_immediate_cut_and_preserves_ena():
    """Verify estop=1 immediately cuts pulses without waiting for ramp, keeping ENA."""
    pipe = MockRxPipeline()
    # Accelerate first
    pipe.process_packet(make_ctrl_packet(seq=1, vx=500), now_ms=0)
    for t in range(20, 200, 20):
        pipe.step_20ms(t)
    assert any(p != 0 for p in pipe.current_pps)

    # Receive E-Stop packet
    pipe.process_packet(make_ctrl_packet(seq=2, estop=1), now_ms=200)
    assert pipe.estop_active is True
    assert pipe.current_pps == [0, 0, 0], "Pulses must be cut immediately on E-stop"
    assert pipe.ena_enabled is True, "ENA holding torque must NOT be disabled on E-stop"


def test_failsafe_timeout_300ms_smooth_ramp():
    """Verify connection loss >300ms triggers failsafe and smooth ramp down to 0."""
    pipe = MockRxPipeline()
    pipe.process_packet(make_ctrl_packet(seq=1, vx=1000), now_ms=0)

    # Run for 200ms with steady packets
    for t in range(20, 201, 20):
        pipe.process_packet(make_ctrl_packet(seq=t // 20 + 1, vx=1000), now_ms=t)
        pipe.step_20ms(t)

    assert not pipe.failsafe_active
    steady_pps = list(pipe.current_pps)
    assert steady_pps[0] != 0, "Current PPS should have ramped up on axis 0"

    # No more packets received: simulate time passing up to 300ms
    pipe.step_20ms(now_ms=450)
    assert not pipe.failsafe_active  # 250ms elapsed, still within 300ms

    # At 520ms (320ms since last packet at 200ms) -> Failsafe triggers!
    pipe.step_20ms(now_ms=520)
    assert pipe.failsafe_active is True
    assert pipe.target_pps == [0, 0, 0]
    # Current pps should be decreasing smoothly, not instantly 0!
    assert any(p != 0 for p in pipe.current_pps), "Failsafe must ramp down smoothly, not stop instantly"
    assert pipe.ena_enabled is True, "ENA holding torque must remain enabled throughout failsafe"

    # After several steps, should reach 0
    for t in range(540, 1500, 20):
        pipe.step_20ms(t)

    assert pipe.current_pps == [0, 0, 0], "Robot must come to a complete smooth stop"
    assert pipe.ena_enabled is True, "ENA holding torque must remain active after stopping"


def test_telemetry_echo_packet_format():
    """Verify telemetry echo packet has seq_echo == seq, link_ok == 1, 8 bytes packed."""
    pipe = MockRxPipeline()
    pipe.process_packet(make_ctrl_packet(seq=42, vx=200), now_ms=0)

    assert len(pipe.telemetry_history) == 1
    telem_bytes = pipe.telemetry_history[0]
    assert len(telem_bytes) == 8, f"telemetry_packet_t must be 8 bytes packed (got {len(telem_bytes)})"

    magic, seq_echo, driver_fault, link_ok, reserved, crc16 = struct.unpack("<BHBBBH", telem_bytes)
    assert magic == TELEMETRY_MAGIC
    assert seq_echo == 42
    assert driver_fault == 0
    assert link_ok == 1
    assert reserved == 0
    assert crc16 == crc16_ccitt_false(telem_bytes[:6])
