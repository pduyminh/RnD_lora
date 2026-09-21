#ifndef AXIS_DRIVER_H
#define AXIS_DRIVER_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Định nghĩa chân GPIO theo mục 1.2 knowledge.md */
#define AXIS_A_PUL_GPIO     4
#define AXIS_A_DIR_GPIO     5
#define AXIS_A_ENA_GPIO     17

#define AXIS_B_PUL_GPIO     6
#define AXIS_B_DIR_GPIO     7
#define AXIS_B_ENA_GPIO     18

#define AXIS_C_PUL_GPIO     15
#define AXIS_C_DIR_GPIO     16
#define AXIS_C_ENA_GPIO     8

#define NUM_AXES            3

/* Mức tích cực cho ENA theo cấu hình sinking optocoupler common-anode 5V (mục 1.3) */
#define ENA_ACTIVE_LEVEL    0   // Mức LOW = tích cực opto = có lực giữ
#define ENA_INACTIVE_LEVEL  1   // Mức HIGH = ngắt opto = thả tự do

/**
 * @brief Khởi tạo toàn bộ driver MCPWM cho 3 trục PUL/DIR và GPIO cho 3 chân ENA.
 * Tự động kích hoạt axis_enable_all(true) lúc boot để giữ cứng vị trí ban đầu.
 */
esp_err_t axis_driver_init(void);

/**
 * @brief Thiết lập tần số xung (pulses_per_sec) và chiều quay cho trục cụ thể.
 * Tần số được cập nhật mượt mà qua MCPWM timer, không dùng vTaskDelay / bit-banging.
 *
 * @param axis Chỉ số trục (0: Trục A, 1: Trục B, 2: Trục C)
 * @param pulses_per_sec Tần số xung mong muốn (xung/giây). Nếu <= 0 sẽ ngắt phát xung.
 * @param dir_positive Chiều quay dương (true) hoặc âm (false).
 */
void axis_set_speed(int axis, int32_t pulses_per_sec, bool dir_positive);

/**
 * @brief Cắt xung lập tức trên cả 3 trục (E-stop dừng động lực khẩn cấp).
 * TUYỆT ĐỐI KHÔNG thay đổi trạng thái chân ENA (bảo toàn lực giữ trục).
 */
void axis_stop_all(void);

/**
 * @brief Điều khiển bật/tắt lực giữ ENA độc lập cho từng trục.
 *
 * @param axis Chỉ số trục (0, 1, 2)
 * @param enable true = cấp lực giữ (khóa cứng trục), false = thả tự do
 */
void axis_set_enable(int axis, bool enable);

/**
 * @brief Điều khiển bật/tắt lực giữ ENA đồng thời cho cả 3 trục.
 *
 * @param enable true = cấp lực giữ cả 3 trục, false = thả tự do
 */
void axis_enable_all(bool enable);

/**
 * @brief Lấy trạng thái lực giữ ENA của trục (phục vụ test và giám sát).
 */
bool axis_get_enable(int axis);

/**
 * @brief Lấy tốc độ xung hiện tại của trục (pps).
 */
int32_t axis_get_speed(int axis);

#ifdef __cplusplus
}
#endif

#endif /* AXIS_DRIVER_H */
