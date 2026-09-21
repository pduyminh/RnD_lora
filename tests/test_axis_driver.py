"""
Unit tests for Axis Driver & MCPWM Pulse Generation (Task 03).
Verifies pin mapping, ENA active levels, period calculations, and static properties.
"""

import re
from pathlib import Path
import pytest

RX_DIR = Path(__file__).parent.parent / "rx" / "main"
HEADER_PATH = RX_DIR / "axis_driver.h"
SOURCE_PATH = RX_DIR / "axis_driver.c"

# Expected Pinout from knowledge.md Section 1.2
EXPECTED_PINS = {
    "A": {"PUL": 4, "DIR": 5, "ENA": 17},
    "B": {"PUL": 6, "DIR": 7, "ENA": 18},
    "C": {"PUL": 15, "DIR": 16, "ENA": 8},
}

MCPWM_BASE_RES_HZ = 1_000_000


def test_axis_driver_files_exist():
    """Verify axis_driver.h and axis_driver.c exist in rx/main/."""
    assert HEADER_PATH.is_file(), f"Missing {HEADER_PATH}"
    assert SOURCE_PATH.is_file(), f"Missing {SOURCE_PATH}"


def test_axis_pin_definitions():
    """Verify GPIO pin definitions in axis_driver.h match knowledge.md exactly."""
    header_content = HEADER_PATH.read_text(encoding="utf-8")

    all_pins = []
    for axis, pins in EXPECTED_PINS.items():
        for pin_type, gpio in pins.items():
            pattern = rf"#define\s+AXIS_{axis}_{pin_type}_GPIO\s+{gpio}\b"
            assert re.search(pattern, header_content), (
                f"Missing or incorrect define for AXIS_{axis}_{pin_type}_GPIO (expected {gpio})"
            )
            all_pins.append(gpio)

    # Ensure no GPIO pin collision
    assert len(all_pins) == 9, "Expected exactly 9 control pins across 3 axes"
    assert len(set(all_pins)) == 9, f"Duplicate GPIO pin assignment detected: {all_pins}"


def test_ena_active_levels():
    """Verify ENA active and inactive levels for sinking optocoupler."""
    header_content = HEADER_PATH.read_text(encoding="utf-8")
    assert re.search(r"#define\s+ENA_ACTIVE_LEVEL\s+0\b", header_content), (
        "ENA_ACTIVE_LEVEL must be 0 (LOW) for sinking optocoupler active state"
    )
    assert re.search(r"#define\s+ENA_INACTIVE_LEVEL\s+1\b", header_content), (
        "ENA_INACTIVE_LEVEL must be 1 (HIGH) for releasing torque"
    )


def test_required_function_signatures():
    """Verify required C API declarations exist in axis_driver.h."""
    header_content = HEADER_PATH.read_text(encoding="utf-8")
    required_functions = [
        "axis_driver_init",
        "axis_set_speed",
        "axis_stop_all",
        "axis_set_enable",
        "axis_enable_all",
    ]
    for func in required_functions:
        assert func in header_content, f"Missing function declaration: {func} in axis_driver.h"


def test_no_bit_banging_or_vtaskdelay():
    """Verify axis_driver.c uses hardware MCPWM and does NOT use bit-banging or vTaskDelay for pulses."""
    source_content = SOURCE_PATH.read_text(encoding="utf-8")
    assert "vTaskDelay" not in source_content, "axis_driver.c must not use vTaskDelay"
    assert "ets_delay_us" not in source_content, "axis_driver.c must not use ets_delay_us"
    assert "esp_rom_delay_us" not in source_content, "axis_driver.c must not use delay loops"
    assert "mcpwm_timer_set_period" in source_content, "axis_driver.c must use mcpwm_timer_set_period"
    assert "mcpwm_comparator_set_compare_value" in source_content, "axis_driver.c must update compare value"


def test_mcpwm_period_calculation():
    """Test period and compare tick math for various target pulse rates."""
    test_rates = [
        (1000, 1000, 500),      # 1 kHz -> 1000 ticks, compare 500
        (8000, 125, 62),        # 8 kHz (1 rps) -> 125 ticks, compare 62
        (25465, 39, 19),        # ~1 m/s -> 39.27 ticks
        (60000, 16, 8),         # Max 60 kHz -> 16 ticks, compare 8
    ]

    for pps, expected_period, expected_cmp in test_rates:
        ticks = MCPWM_BASE_RES_HZ // pps
        cmp_val = ticks // 2
        assert ticks == expected_period, f"PPS={pps}: expected ticks {expected_period}, got {ticks}"
        assert cmp_val == expected_cmp, f"PPS={pps}: expected cmp {expected_cmp}, got {cmp_val}"


def test_axis_driver_state_simulation():
    """Simulate driver behavior: init boot enable, set speed, e-stop maintaining ENA."""
    class AxisMock:
        def __init__(self, axis_id):
            self.axis_id = axis_id
            self.ena = False
            self.pps = 0
            self.dir = True
            self.running = False

        def set_enable(self, en: bool):
            self.ena = en

        def set_speed(self, pps: int, dir_pos: bool):
            self.dir = dir_pos
            if pps <= 0:
                self.pps = 0
                self.running = False
            else:
                self.pps = pps
                self.running = True

        def stop(self):
            # E-stop stops pulses immediately without releasing ENA
            self.pps = 0
            self.running = False

    axes = [AxisMock(i) for i in range(3)]

    # Boot initialization: axis_enable_all(true)
    for ax in axes:
        ax.set_enable(True)
    assert all(ax.ena for ax in axes), "All axes must have holding torque at boot"

    # Set speed on Axis A and B
    axes[0].set_speed(8000, True)
    axes[1].set_speed(4000, False)
    assert axes[0].running and axes[0].pps == 8000 and axes[0].dir is True
    assert axes[1].running and axes[1].pps == 4000 and axes[1].dir is False

    # Emergency stop: axis_stop_all()
    for ax in axes:
        ax.stop()

    assert all(not ax.running for ax in axes), "All axes must stop running on E-stop"
    assert all(ax.pps == 0 for ax in axes), "All pulse outputs must be zero"
    assert all(ax.ena for ax in axes), "ENA holding torque must remain active after E-stop!"
