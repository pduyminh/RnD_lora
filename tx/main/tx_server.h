#ifndef TX_SERVER_H
#define TX_SERVER_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cấu hình WiFi SoftAP theo tiêu chí chấp nhận (dễ cấu hình đầu file) */
#define TX_AP_SSID              "OMNI_ROBOT_TX"
#define TX_AP_PASSWORD          "12345678"
#define TX_AP_CHANNEL           1
#define TX_AP_MAX_CONN          4

/* Chu kỳ gửi ESP-NOW 50 Hz (20 ms ± 2 ms) */
#define TX_ESPNOW_PERIOD_MS     20

/**
 * @brief Khởi tạo SoftAP, HTTP Web Server, WebSocket và task phát ESP-NOW 50Hz.
 */
esp_err_t tx_server_init(void);

/**
 * @brief Cập nhật vận tốc điều khiển từ WebSocket hoặc nguồn khác (thread-safe).
 *
 * @param vx_mm_s Vận tốc trục X (mm/s)
 * @param vy_mm_s Vận tốc trục Y (mm/s)
 * @param omega_mrad_s Vận tốc góc (mrad/s)
 * @param estop Cờ dừng khẩn cấp (1 = dừng)
 */
void tx_set_motion_command(int16_t vx_mm_s, int16_t vy_mm_s, int16_t omega_mrad_s, uint8_t estop);

/**
 * @brief Lấy trạng thái số lượng client WebSocket đang kết nối.
 */
uint32_t tx_get_ws_client_count(void);

/**
 * @brief Lấy sequence number của gói tin đã gửi gần nhất.
 */
uint16_t tx_get_current_seq(void);

/**
 * @brief Tạo và đóng gói ctrl_packet_t (phục vụ cả task phát và unit test).
 */
void tx_build_ctrl_packet(ctrl_packet_t *pkt);

/**
 * @brief Lấy giá trị vận tốc vx hiện tại (mm/s).
 */
int16_t tx_get_vx(void);

/**
 * @brief Đặt cờ E-Stop phần mềm từ nút bấm vật lý TM1638.
 */
void tx_set_estop(uint8_t estop);

/**
 * @brief Lấy cờ E-Stop hiện tại.
 */
uint8_t tx_get_estop(void);

/**
 * @brief Gửi ngay 1 gói tin ctrl_packet_t qua ESP-NOW không cần chờ chu kỳ 20ms.
 */
esp_err_t tx_send_packet_now(void);

#ifdef __cplusplus
}
#endif

#endif /* TX_SERVER_H */
