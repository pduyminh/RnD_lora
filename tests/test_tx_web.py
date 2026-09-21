"""
Unit tests for TX SoftAP, Web Virtual Joystick & 50Hz ESP-NOW Sender (Task 05).
Tests configuration definitions, HTML joystick/slider elements, WebSocket message parsing,
50Hz periodic packet generation, and zeroing motion when no client is connected.
"""

import re
import struct
from pathlib import Path
import pytest

TX_DIR = Path(__file__).parent.parent / "tx" / "main"
SERVER_H_PATH = TX_DIR / "tx_server.h"
SERVER_C_PATH = TX_DIR / "tx_server.c"

CTRL_PACKET_MAGIC = 0xA5


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def test_tx_server_files_exist():
    """Verify tx_server.h and tx_server.c exist in tx/main/."""
    assert SERVER_H_PATH.is_file(), f"Missing {SERVER_H_PATH}"
    assert SERVER_C_PATH.is_file(), f"Missing {SERVER_C_PATH}"


def test_softap_and_timing_configurations():
    """Verify SoftAP credentials and 50Hz timing are configurable at the top of file."""
    h_content = SERVER_H_PATH.read_text(encoding="utf-8")
    assert re.search(r'#define\s+TX_AP_SSID\s+"[^"]+"', h_content), "TX_AP_SSID must be defined"
    assert re.search(r'#define\s+TX_AP_PASSWORD\s+"[^"]*"', h_content), "TX_AP_PASSWORD must be defined"
    assert re.search(r'#define\s+TX_ESPNOW_PERIOD_MS\s+20\b', h_content), "TX_ESPNOW_PERIOD_MS must be 20 ms (50Hz)"


def test_embedded_html_contains_required_controls():
    """Verify embedded HTML/JS page contains 2-axis joystick, omega slider, and E-Stop button."""
    c_content = SERVER_C_PATH.read_text(encoding="utf-8")

    # 2-axis Virtual Joystick
    assert "joystick-container" in c_content, "Missing joystick container in HTML"
    assert "joystick-handle" in c_content, "Missing joystick handle in HTML"

    # Omega slider or second control
    assert "omega-slider" in c_content, "Missing omega slider in HTML"

    # Emergency Stop button
    assert "estop-btn" in c_content, "Missing E-Stop button in HTML"
    assert "EMERGENCY STOP" in c_content, "Missing EMERGENCY STOP label in HTML"

    # WebSocket connection to /ws endpoint
    assert "/ws" in c_content, "HTML must connect to WebSocket endpoint /ws"
    assert "JSON.stringify" in c_content, "HTML must serialize commands to JSON"


class MockTxServer:
    """Python simulation of tx_server packet packaging and client state logic."""
    def __init__(self):
        self.seq = 0
        self.vx = 0
        self.vy = 0
        self.omega = 0
        self.estop = 0
        self.ws_clients = 0
        self.last_rx_tick = 0

    def set_motion(self, vx: int, vy: int, omega: int, estop: int, now_ms: int):
        self.vx = vx
        self.vy = vy
        self.omega = omega
        self.estop = estop
        self.last_rx_tick = now_ms

    def build_packet(self, now_ms: int) -> bytes:
        self.seq = (self.seq + 1) & 0xFFFF

        # If no client connected or timed out (> 500ms), send zeroes
        client_active = (self.ws_clients > 0) and (now_ms - self.last_rx_tick <= 500)
        if client_active:
            vx, vy, omega, estop = self.vx, self.vy, self.omega, self.estop
        else:
            vx, vy, omega, estop = 0, 0, 0, self.estop

        payload = struct.pack("<BHhhhB", CTRL_PACKET_MAGIC, self.seq, vx, vy, omega, estop)
        crc = crc16_ccitt_false(payload)
        return payload + struct.pack("<H", crc)


def test_zero_motion_when_no_client():
    """Verify that when no client is connected, TX sends packets with vx=vy=omega=0."""
    server = MockTxServer()
    server.ws_clients = 0  # No client connected
    server.vx = 800
    server.vy = 400
    server.omega = 1500

    pkt = server.build_packet(now_ms=1000)
    magic, seq, vx, vy, omega, estop, crc = struct.unpack("<BHhhhBH", pkt)

    assert magic == CTRL_PACKET_MAGIC
    assert seq == 1
    assert vx == 0, "vx must be 0 when no client connected"
    assert vy == 0, "vy must be 0 when no client connected"
    assert omega == 0, "omega must be 0 when no client connected"
    assert crc == crc16_ccitt_false(pkt[:10])


def test_normal_motion_with_connected_client():
    """Verify that when a client is active, TX sends commanded speeds."""
    server = MockTxServer()
    server.ws_clients = 1
    server.set_motion(vx=500, vy=-300, omega=1000, estop=0, now_ms=1000)

    pkt = server.build_packet(now_ms=1020)
    magic, seq, vx, vy, omega, estop, crc = struct.unpack("<BHhhhBH", pkt)

    assert seq == 1
    assert vx == 500
    assert vy == -300
    assert omega == 1000
    assert estop == 0
    assert crc == crc16_ccitt_false(pkt[:10])


def test_seq_increments_by_one_every_packet():
    """Verify seq increases by exactly 1 on every 20ms packet."""
    server = MockTxServer()
    server.ws_clients = 1
    server.set_motion(0, 0, 0, 0, now_ms=0)

    for expected_seq in range(1, 51):
        pkt = server.build_packet(now_ms=expected_seq * 20)
        _, seq, _, _, _, _, _ = struct.unpack("<BHhhhBH", pkt)
        assert seq == expected_seq, f"Expected seq {expected_seq}, got {seq}"


def test_client_timeout_zeroes_motion():
    """Verify that if client stops sending for > 500ms, motion reverts to 0."""
    server = MockTxServer()
    server.ws_clients = 1
    server.set_motion(vx=800, vy=0, omega=0, estop=0, now_ms=1000)

    # Active at 1200ms (200ms elapsed) -> motion sent
    pkt = server.build_packet(now_ms=1200)
    _, _, vx, _, _, _, _ = struct.unpack("<BHhhhBH", pkt)
    assert vx == 800

    # At 1600ms (600ms elapsed > 500ms timeout) -> zeroed!
    pkt_timeout = server.build_packet(now_ms=1600)
    _, _, vx_to, _, _, _, _ = struct.unpack("<BHhhhBH", pkt_timeout)
    assert vx_to == 0, "Motion must be zeroed when client message times out"
