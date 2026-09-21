#include "kinematics.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* sqrt(3) / 2 dùng cho các góc 120° và 240° */
static const float SQRT3_OVER_2 = 0.8660254037844386f;

void kinematics_clamp_input(float *vx, float *vy, float *omega)
{
    if (!vx || !vy || !omega) {
        return;
    }

    /* 1. Giới hạn độ lớn vector vận tốc tịnh tiến |v| <= 1.0 m/s */
    float v_mag = sqrtf((*vx) * (*vx) + (*vy) * (*vy));
    if (v_mag > MAX_TRANSLATIONAL_SPEED_M_S) {
        float scale = MAX_TRANSLATIONAL_SPEED_M_S / v_mag;
        *vx *= scale;
        *vy *= scale;
    }

    /* 2. Giới hạn độ lớn vận tốc góc |omega| <= 2.0 rad/s */
    if (*omega > MAX_ANGULAR_SPEED_RAD_S) {
        *omega = MAX_ANGULAR_SPEED_RAD_S;
    } else if (*omega < -MAX_ANGULAR_SPEED_RAD_S) {
        *omega = -MAX_ANGULAR_SPEED_RAD_S;
    }
}

void kinematics_scale_proportional(int32_t pps[3], int32_t max_pps_limit)
{
    if (!pps || max_pps_limit <= 0) {
        return;
    }

    int32_t max_abs = 0;
    for (int i = 0; i < 3; i++) {
        int32_t val = labs(pps[i]);
        if (val > max_abs) {
            max_abs = val;
        }
    }

    if (max_abs > max_pps_limit) {
        float factor = (float)max_pps_limit / (float)max_abs;
        for (int i = 0; i < 3; i++) {
            pps[i] = (int32_t)lrintf((float)pps[i] * factor);
        }
    }
}

void kinematics_compute(float vx, float vy, float omega, int32_t out_pps[3])
{
    if (!out_pps) {
        return;
    }

    /* Clamp giới hạn đầu vào trước khi tính toán */
    kinematics_clamp_input(&vx, &vy, &omega);

    /*
     * Công thức động học nghịch robot 3 bánh omni mục 2 knowledge.md:
     * v_wheel_i (m/s) = -sin(theta_i) * vx + cos(theta_i) * vy + L * omega
     * - Trục A (bánh 0 tại 0°):   sin(0)   = 0,            cos(0)   = 1
     *   => v0 = vy + L * omega
     * - Trục B (bánh 1 tại 120°): sin(120) = sqrt(3)/2,    cos(120) = -0.5
     *   => v1 = -sqrt(3)/2 * vx - 0.5 * vy + L * omega
     * - Trục C (bánh 2 tại 240°): sin(240) = -sqrt(3)/2,   cos(240) = -0.5
     *   => v2 =  sqrt(3)/2 * vx - 0.5 * vy + L * omega
     */
    float v_rot = ROBOT_L_M * omega;
    float v0 = vy + v_rot;
    float v1 = -SQRT3_OVER_2 * vx - 0.5f * vy + v_rot;
    float v2 =  SQRT3_OVER_2 * vx - 0.5f * vy + v_rot;

    /* Quy đổi sang xung/giây dựa theo tỷ lệ 8000 / (2 * pi * r) */
    out_pps[0] = (int32_t)lrintf(v0 * PULSES_PER_METER);
    out_pps[1] = (int32_t)lrintf(v1 * PULSES_PER_METER);
    out_pps[2] = (int32_t)lrintf(v2 * PULSES_PER_METER);

    /* Co tỉ lệ đồng dạng nếu có bánh vượt quá ngưỡng trần WHEEL_MAX_PPS */
    kinematics_scale_proportional(out_pps, WHEEL_MAX_PPS);
}

void kinematics_ramp_reset(void)
{
    /* Trạng thái được quản lý trực tiếp trong mảng current_pps của caller */
}

void kinematics_ramp_update(const int32_t target_pps[3], int32_t current_pps[3], float dt_s)
{
    if (!target_pps || !current_pps) {
        return;
    }
    if (dt_s <= 0.0f) {
        dt_s = DEFAULT_RAMP_DT_S;
    }

    /* Bước gia tốc chuẩn ứng với gia tốc tối đa 2.0 m/s^2 */
    float max_step_base = MAX_ACCEL_M_S2 * PULSES_PER_METER * dt_s;
    if (max_step_base < 1.0f) {
        max_step_base = 1.0f;
    }

    for (int i = 0; i < 3; i++) {
        int32_t target = target_pps[i];
        int32_t current = current_pps[i];
        int32_t diff = target - current;

        if (diff == 0) {
            continue;
        }

        /*
         * Giải thuật Anti-Jerk (mượt hóa S-curve):
         * Khi vận tốc hiện tại gần 0 (lúc bắt đầu đề-pa hoặc lúc chuẩn bị dừng hẳn),
         * giảm độ dốc gia tốc để tránh xung lực đột ngột gây giật cơ khí.
         */
        float cur_abs = (float)labs(current);
        float step_limit = max_step_base;

        if (cur_abs < (float)ANTI_JERK_THRESHOLD_PPS) {
            float ratio = cur_abs / (float)ANTI_JERK_THRESHOLD_PPS;
            /* Hệ số làm mịn từ 0.25 tại tốc độ 0 đến 1.0 tại ngưỡng ANTI_JERK_THRESHOLD_PPS */
            float ease = 0.25f + 0.75f * (ratio * ratio);
            step_limit = max_step_base * ease;
            if (step_limit < 100.0f) {
                step_limit = 100.0f; /* Đảm bảo bước tiến tối thiểu để không bị trễ */
            }
        }

        int32_t step = (int32_t)lrintf(step_limit);
        if (step < 1) {
            step = 1;
        }

        if (labs(diff) <= step) {
            current_pps[i] = target;
        } else if (diff > 0) {
            current_pps[i] += step;
        } else {
            current_pps[i] -= step;
        }
    }
}
