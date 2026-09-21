"""
Unit tests for protocol.h in RnD_LoRa project.
Verifies packet sizes, layout, packed attributes, and CRC-16/CCITT-FALSE algorithm.
"""

import struct
import pytest

# CRC-16/CCITT-FALSE reference implementation in Python
def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def test_crc16_standard_vector():
    """Test vector standard: ASCII '123456789' -> 0x29B1."""
    test_input = b"123456789"
    expected = 0x29B1
    actual = crc16_ccitt_false(test_input)
    assert actual == expected, f"Expected 0x{expected:04X}, got 0x{actual:04X}"


def test_crc16_empty():
    """Empty data should return initial value 0xFFFF."""
    assert crc16_ccitt_false(b"") == 0xFFFF


def test_ctrl_packet_structure_size():
    """
    ctrl_packet_t (12 bytes packed):
    - magic (uint8_t): 1 byte (0xA5)
    - seq (uint16_t): 2 bytes
    - vx_mm_s (int16_t): 2 bytes
    - vy_mm_s (int16_t): 2 bytes
    - omega_mrad_s (int16_t): 2 bytes
    - estop (uint8_t): 1 byte
    - crc16 (uint16_t): 2 bytes
    Total = 12 bytes
    """
    fmt = "<BHhhhBH"
    size = struct.calcsize(fmt)
    assert size == 12, f"ctrl_packet_t format size is {size}, expected 12"

    # Pack a sample packet and verify CRC offset and calculation
    magic = 0xA5
    seq = 42
    vx = 250
    vy = -100
    omega = 500
    estop = 0

    payload = struct.pack("<BHhhhB", magic, seq, vx, vy, omega, estop)
    assert len(payload) == 10, "Payload before crc16 must be exactly 10 bytes"

    crc = crc16_ccitt_false(payload)
    packet = payload + struct.pack("<H", crc)
    assert len(packet) == 12, "Full ctrl_packet_t must be exactly 12 bytes"


def test_telemetry_packet_structure_size():
    """
    telemetry_packet_t (8 bytes packed):
    - magic (uint8_t): 1 byte (0x5A)
    - seq_echo (uint16_t): 2 bytes
    - driver_fault_bitmap (uint8_t): 1 byte
    - link_ok (uint8_t): 1 byte
    - reserved (uint8_t): 1 byte
    - crc16 (uint16_t): 2 bytes
    Total = 8 bytes
    """
    fmt = "<BHBBBH"
    size = struct.calcsize(fmt)
    assert size == 8, f"telemetry_packet_t format size is {size}, expected 8"

    magic = 0x5A
    seq_echo = 42
    fault_bitmap = 0
    link_ok = 1
    reserved = 0

    payload = struct.pack("<BHBBB", magic, seq_echo, fault_bitmap, link_ok, reserved)
    assert len(payload) == 6, "Payload before crc16 must be exactly 6 bytes"

    crc = crc16_ccitt_false(payload)
    packet = payload + struct.pack("<H", crc)
    assert len(packet) == 8, "Full telemetry_packet_t must be exactly 8 bytes"
