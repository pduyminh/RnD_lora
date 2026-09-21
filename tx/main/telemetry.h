#ifndef TELEMETRY_H
#define TELEMETRY_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Ngưỡng timeout mất liên kết telemetry (500ms theo tiêu chí T07, lớn hơn failsafe 300ms của RX) */
#define TELEMETRY_TIMEOUT_MS       500

/**
 * @brief Khởi tạo hệ thống nhận và giám sát Telemetry trên TX.
 *        Đăng ký callback ESP-NOW nhận gói tin và tạo background task giám sát timeout.
 */
esp_err_t telemetry_init(void);

/**
 * @brief Xử lý gói tin telemetry nhận được từ ESP-NOW (phục vụ cả runtime và unit test).
 *        Validate magic = 0x5A, CRC-16/CCITT-FALSE và cập nhật thời điểm nhận gần nhất.
 *
 * @param data Con trỏ dữ liệu gói tin nhận được
 * @param len Độ dài dữ liệu nhận được
 * @return true nếu gói tin hợp lệ, false nếu bị loại bỏ.
 */
bool telemetry_process_packet(const uint8_t *data, size_t len);

/**
 * @brief Kiểm tra trạng thái liên kết RX hiện tại (true = OK, false = MẤT KẾT NỐI).
 */
bool telemetry_is_link_ok(void);

/**
 * @brief Lấy số sequence number echo gần nhất mà RX phản hồi.
 */
uint16_t telemetry_get_last_seq_echo(void);

/**
 * @brief Kiểm tra và cập nhật trạng thái timeout 500ms.
 */
void telemetry_check_timeout(void);

/**
 * @brief Lấy tổng số gói telemetry hợp lệ đã nhận.
 */
uint32_t telemetry_get_rx_count(void);

/**
 * @brief Lấy tổng số gói telemetry bị loại bỏ do sai magic hoặc CRC.
 */
uint32_t telemetry_get_dropped_count(void);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
