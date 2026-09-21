#ifndef COMM_H
#define COMM_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAILSAFE_TIMEOUT_MS     300
#define PIPELINE_PERIOD_MS      20

/**
 * @brief Khởi tạo module truyền thông ESP-NOW và pipeline điều khiển cho RX.
 * Tự động cấu hình NVS, WiFi STA mode và đăng ký callback nhận/gửi ESP-NOW.
 */
esp_err_t comm_init(void);

/**
 * @brief Hàm chu kỳ xử lý pipeline điều khiển (chu kỳ 20ms / 50Hz).
 * Kiểm tra gói mới, kiểm tra timeout failsafe 300ms, tính toán kinematics,
 * điều khiển ramp mượt mà và xuất xung tới axis_driver.
 */
void comm_pipeline_step(void);

/**
 * @brief Xử lý một gói tin raw nhận được (phục vụ cả ESP-NOW callback và unit test / mock injection).
 *
 * @param src_mac Địa chỉ MAC nguồn (6 bytes)
 * @param data Con trỏ dữ liệu gói tin
 * @param data_len Độ dài dữ liệu nhận được
 * @return true nếu gói tin hợp lệ (đúng magic, CRC, seq mới hơn), false nếu bị loại
 */
bool comm_process_packet(const uint8_t *src_mac, const uint8_t *data, size_t data_len);

/**
 * @brief Lấy số lượng gói tin bị loại bỏ (sai magic, sai CRC hoặc seq cũ/lặp).
 */
uint32_t comm_get_dropped_packet_count(void);

/**
 * @brief Lấy seq của gói tin hợp lệ gần nhất đã xử lý.
 */
uint16_t comm_get_last_seq(void);

/**
 * @brief Kiểm tra trạng thái failsafe timeout (mất kết nối > 300ms).
 */
bool comm_is_failsafe_active(void);

/**
 * @brief Kiểm tra trạng thái E-Stop phần mềm (nhận estop=1 từ TX).
 */
bool comm_is_estop_active(void);

#ifdef __cplusplus
}
#endif

#endif /* COMM_H */
