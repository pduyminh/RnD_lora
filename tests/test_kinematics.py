"""
Unit tests for 3-wheel omni kinematics and smooth anti-jerk ramping (Task 02).
"""

import math
import pytest

# Constants from knowledge.md and kinematics.h
ROBOT_L_M = 0.38
WHEEL_RADIUS_M = 0.05
PULSES_PER_REV = 8000.0
PULSES_PER_METER = PULSES_PER_REV / (2.0 * math.pi * WHEEL_RADIUS_M)  # ~25464.7909
MAX_TRANSLATIONAL_SPEED_M_S = 1.0
MAX_ANGULAR_SPEED_RAD_S = 2.0
WHEEL_MAX_PPS = 60000
MAX_ACCEL_M_S2 = 2.0
DEFAULT_RAMP_DT_S = 0.02
MAX_DELTA_PPS_PER_20MS = round(MAX_ACCEL_M_S2 * PULSES_PER_METER * DEFAULT_RAMP_DT_S)  # ~1019


def clamp_input(vx: float, vy: float, omega: float):
    v_mag = math.hypot(vx, vy)
    if v_mag > MAX_TRANSLATIONAL_SPEED_M_S:
        scale = MAX_TRANSLATIONAL_SPEED_M_S / v_mag
        vx *= scale
        vy *= scale

    if omega > MAX_ANGULAR_SPEED_RAD_S:
        omega = MAX_ANGULAR_SPEED_RAD_S
    elif omega < -MAX_ANGULAR_SPEED_RAD_S:
        omega = -MAX_ANGULAR_SPEED_RAD_S

    return vx, vy, omega


def scale_proportional(pps: list[int], max_pps: int = WHEEL_MAX_PPS) -> list[int]:
    max_abs = max(abs(p) for p in pps)
    if max_abs > max_pps:
        factor = max_pps / max_abs
        return [round(p * factor) for p in pps]
    return list(pps)


def compute_kinematics(vx: float, vy: float, omega: float) -> list[int]:
    vx, vy, omega = clamp_input(vx, vy, omega)

    sqrt3_2 = math.sqrt(3) / 2.0
    v_rot = ROBOT_L_M * omega
    v0 = vy + v_rot
    v1 = -sqrt3_2 * vx - 0.5 * vy + v_rot
    v2 =  sqrt3_2 * vx - 0.5 * vy + v_rot

    raw_pps = [
        round(v0 * PULSES_PER_METER),
        round(v1 * PULSES_PER_METER),
        round(v2 * PULSES_PER_METER)
    ]
    return scale_proportional(raw_pps, WHEEL_MAX_PPS)


def ramp_step(target: int, current: int, dt: float = 0.02) -> int:
    diff = target - current
    if diff == 0:
        return current

    max_step_base = MAX_ACCEL_M_S2 * PULSES_PER_METER * dt
    cur_abs = abs(current)
    step_limit = max_step_base

    anti_jerk_threshold = 5000.0
    if cur_abs < anti_jerk_threshold:
        ratio = cur_abs / anti_jerk_threshold
        ease = 0.25 + 0.75 * (ratio * ratio)
        step_limit = max(100.0, max_step_base * ease)

    step = round(step_limit)
    if abs(diff) <= step:
        return target
    elif diff > 0:
        return current + step
    else:
        return current - step


def test_input_clamping():
    # Translation exceeding 1.0 m/s
    vx, vy, w = clamp_input(2.0, 0.0, 0.0)
    assert pytest.approx(vx, abs=1e-5) == 1.0
    assert pytest.approx(vy, abs=1e-5) == 0.0

    # Angular exceeding 2.0 rad/s
    vx, vy, w = clamp_input(0.0, 0.0, 3.5)
    assert w == 2.0

    vx, vy, w = clamp_input(0.0, 0.0, -3.5)
    assert w == -2.0

    # Vector 45 degrees exceeding magnitude 1.0
    vx, vy, w = clamp_input(1.0, 1.0, 0.0)
    assert pytest.approx(math.hypot(vx, vy), abs=1e-5) == 1.0


def test_test_vector_1():
    # vx = 1.0, vy = 0.0, omega = 0.0
    pps = compute_kinematics(1.0, 0.0, 0.0)
    assert pps[0] == 0
    assert abs(pps[1] - (-22053)) <= 1
    assert abs(pps[2] - 22053) <= 1


def test_test_vector_2():
    # vx = 0.0, vy = 0.0, omega = 2.0
    pps = compute_kinematics(0.0, 0.0, 2.0)
    assert abs(pps[0] - 19353) <= 1
    assert abs(pps[1] - 19353) <= 1
    assert abs(pps[2] - 19353) <= 1


def test_test_vector_3():
    # vx = 0.5, vy = 0.5, omega = 1.0
    pps = compute_kinematics(0.5, 0.5, 1.0)
    assert abs(pps[0] - 22409) <= 1
    assert abs(pps[1] - (-7716)) <= 1
    assert abs(pps[2] - 14337) <= 1


def test_proportional_scaling():
    over_pps = [70000, 35000, -70000]
    scaled = scale_proportional(over_pps, WHEEL_MAX_PPS)
    assert max(abs(p) for p in scaled) == WHEEL_MAX_PPS
    assert scaled[0] == 60000
    assert scaled[1] == 30000
    assert scaled[2] == -60000


def test_smooth_ramp_acceleration_limit():
    target = 22053
    current = 0
    history = [current]
    for _ in range(50):
        current = ramp_step(target, current, 0.02)
        history.append(current)
        if current == target:
            break

    # First step should have reduced acceleration (anti-jerk)
    first_step = history[1] - history[0]
    assert first_step < MAX_DELTA_PPS_PER_20MS
    assert first_step >= 100

    # Intermediate steps should never exceed MAX_DELTA_PPS_PER_20MS + 2 (tolerance)
    for i in range(1, len(history)):
        diff = abs(history[i] - history[i-1])
        assert diff <= MAX_DELTA_PPS_PER_20MS + 2

    # Eventually reaches target
    assert history[-1] == target
