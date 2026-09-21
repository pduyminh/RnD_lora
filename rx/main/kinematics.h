#ifndef KINEMATICS_H
#define KINEMATICS_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Thông số hình học robot 3 bánh omni theo mục 1.4 knowledge.md */
#define ROBOT_L_M                   0.38f        // Khoảng cách từ tâm robot đến tâm bánh (m)
#define WHEEL_RADIUS_M              0.05f        // Bán kính bánh xe r (m)
#define PULSES_PER_REV              8000.0f      // Số xung/vòng (vi bước 40)
#define PULSES_PER_METER            25464.7909f  // 8000 / (2 * PI * 0.05) ~ 25464.79 xung/m

/* Giới hạn vận tốc đầu vào theo mục 3 knowledge.md */
#define MAX_TRANSLATIONAL_SPEED_M_S 1.0f         // Trần vận tốc tịnh tiến |v| <= 1.0 m/s
#define MAX_ANGULAR_SPEED_RAD_S     2.0f         // Trần vận tốc góc |omega| <= 2.0 rad/s

/* Giới hạn xung/giây tối đa cho mỗi bánh */
#define WHEEL_MAX_PPS               60000        // Hằng số tạm thời theo task.md

/* Giới hạn gia tốc và chu kỳ ramp */
#define MAX_ACCEL_M_S2              2.0f         // Gia tốc tối đa 2.0 m/s^2
#define DEFAULT_RAMP_DT_S           0.02f        // Chu kỳ gọi mặc định 20 ms (50 Hz)
#define MAX_DELTA_PPS_PER_20MS      1018         // 2.0 * 25464.79 * 0.02 ~ 1018.6 xung/chu kỳ

/* Ngưỡng vận tốc cho giải thuật làm mịn (Anti-Jerk) khi khởi động và dừng */
#define ANTI_JERK_THRESHOLD_PPS     5000         // Dưới ngưỡng này, giảm độ dốc gia tốc để chuyển động êm ái

/**
 * @brief Giới hạn đầu vào (vx, vy, omega) theo mục 3 knowledge.md.
 * - Độ lớn vector tịnh tiến sqrt(vx^2 + vy^2) <= 1.0 m/s (co tỉ lệ nếu vượt).
 * - Vận tốc góc omega được kẹp trong [-2.0, 2.0] rad/s.
 */
void kinematics_clamp_input(float *vx, float *vy, float *omega);

/**
 * @brief Tính toán động học nghịch cho 3 bánh omni (góc 0°, 120°, 240°).
 * Áp dụng công thức mục 2 knowledge.md, tự động clamp đầu vào và co tỉ lệ nếu vượt WHEEL_MAX_PPS.
 *
 * @param vx Vận tốc trục X robot (m/s)
 * @param vy Vận tốc trục Y robot (m/s)
 * @param omega Vận tốc góc robot (rad/s)
 * @param out_pps Mảng 3 phần tử chứa số xung/giây xuất ra cho 3 trục [Trục A (0°), Trục B (120°), Trục C (240°)]
 */
void kinematics_compute(float vx, float vy, float omega, int32_t out_pps[3]);

/**
 * @brief Co tỉ lệ đồng dạng cả 3 giá trị pulses/s nếu có bánh vượt ngưỡng max_pps.
 * Giữ nguyên hướng vector chuyển động, chỉ giảm độ lớn.
 */
void kinematics_scale_proportional(int32_t pps[3], int32_t max_pps_limit);

/**
 * @brief Khởi tạo hoặc đặt lại bộ ramp gia tốc về 0.
 */
void kinematics_ramp_reset(void);

/**
 * @brief Cập nhật ramp gia tốc hình thang mượt hóa (Anti-Jerk S-curve) cho 3 trục.
 * Giới hạn tốc độ thay đổi pulses/s mỗi chu kỳ theo gia tốc tối đa 2 m/s^2, đồng thời
 * làm mịn gia tốc khi khởi hành và khi hãm dừng để triệt tiêu lực giật cơ khí.
 *
 * @param target_pps Tốc độ xung/giây mục tiêu từ kinematics_compute
 * @param current_pps Tốc độ xung/giây hiện tại (được cập nhật trực tiếp tại chỗ)
 * @param dt_s Khoảng thời gian chu kỳ tính toán (thường là 0.02s tương đương 20 ms)
 */
void kinematics_ramp_update(const int32_t target_pps[3], int32_t current_pps[3], float dt_s);

#ifdef __cplusplus
}
#endif

#endif /* KINEMATICS_H */
