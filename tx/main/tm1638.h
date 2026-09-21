#ifndef TM1638_H
#define TM1638_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sơ đồ chân TM1638 theo mục 1.2 trong knowledge.md */
#define TM1638_STB_GPIO    GPIO_NUM_4
#define TM1638_CLK_GPIO    GPIO_NUM_5
#define TM1638_DIO_GPIO    GPIO_NUM_6

/* Lệnh TM1638 theo datasheet */
#define TM1638_CMD_DATA_WRITE_AUTO   0x40
#define TM1638_CMD_DATA_READ_KEYS    0x42
#define TM1638_CMD_DATA_WRITE_FIXED  0x44
#define TM1638_CMD_ADDR_START        0xC0
#define TM1638_CMD_DISPLAY_ON        0x88
#define TM1638_CMD_DISPLAY_OFF       0x80

/**
 * @brief Khởi tạo module TM1638 (GPIO STB=4, CLK=5, DIO=6) và tạo task cập nhật.
 */
esp_err_t tm1638_init(void);

/**
 * @brief Bật hoặc tắt 1 trong 8 LED đơn (index 0 đến 7).
 *        LED 0 (LED 1 trên mặt module): chỉ báo E-Stop.
 */
void tm1638_set_led(uint8_t index, bool on);

/**
 * @brief Đặt giá trị 7 đoạn cho 1 vị trí (0 đến 7).
 */
void tm1638_set_digit(uint8_t position, uint8_t segments);

/**
 * @brief Cập nhật nội dung hiển thị 8 LED 7 đoạn:
 *        Digits 0-1: 'C' + client_count (số client kết nối AP/WS)
 *        Digit 2: '-' hoặc ' '
 *        Digits 3-7: giá trị vx hiện tại (mm/s)
 *        LED 1: sáng khi estop=true, tắt khi bình thường.
 */
void tm1638_display_status(uint8_t client_count, int16_t vx_mm_s, bool estop);

/**
 * @brief Đọc trạng thái 8 nút bấm trên TM1638.
 *        Trả về bitmap (bit 0 = nút 1, bit 1 = nút 2, ...).
 */
uint8_t tm1638_read_keys(void);

/**
 * @brief Lấy trạng thái chốt E-stop hiện tại từ TM1638.
 */
bool tm1638_get_estop(void);

/**
 * @brief Đặt trạng thái chốt E-stop từ phần mềm hoặc nút bấm.
 */
void tm1638_set_estop(bool estop);

/**
 * @brief Mã hóa một ký tự sang 7 đoạn (common cathode).
 */
uint8_t tm1638_encode_char(char c);

/**
 * @brief Mã hóa một chữ số 0-9 sang 7 đoạn.
 */
uint8_t tm1638_encode_digit(uint8_t digit);

#ifdef __cplusplus
}
#endif

#endif /* TM1638_H */
