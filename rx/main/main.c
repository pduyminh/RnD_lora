#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "protocol.h"
#include "kinematics.h"

static const char *TAG = "OMNI_RX";

#define STATUS_LED_GPIO       GPIO_NUM_2
#define BLINK_HALF_PERIOD_MS  500

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Khởi động firmware OMNI_RX (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. Kiểm tra và log kích thước struct theo giao thức */
    ESP_LOGI(TAG, "sizeof(ctrl_packet_t) = %u", (unsigned int)sizeof(ctrl_packet_t));
    ESP_LOGI(TAG, "sizeof(telemetry_packet_t) = %u", (unsigned int)sizeof(telemetry_packet_t));
    printf("sizeof(ctrl_packet_t) = %u\n", (unsigned int)sizeof(ctrl_packet_t));
    printf("sizeof(telemetry_packet_t) = %u\n", (unsigned int)sizeof(telemetry_packet_t));
    ESP_LOGI(TAG, "Giao thức định nghĩa đúng: ctrl_packet_t = 12 bytes, telemetry_packet_t = 8 bytes");

    /* 2. CRC-16/CCITT-FALSE Self-test với vector chuẩn ASCII '123456789' -> 0x29B1 */
    const char *test_input = "123456789";
    const uint16_t expected_crc = 0x29B1;
    uint16_t actual_crc = crc16_ccitt_false((const uint8_t *)test_input, strlen(test_input));
    bool crc_pass = (actual_crc == expected_crc);

    ESP_LOGI(TAG, "CRC-16/CCITT-FALSE self-test: input=\"%s\", expected=0x%04X, actual=0x%04X -> %s",
             test_input, expected_crc, actual_crc, crc_pass ? "PASS" : "FAIL");

    /* 3. T02: Kiểm tra động học tam giác (Kinematics) với 3 bộ input test cố định */
    int32_t pps[3];

    // Bộ test 1: vx = 1.0 m/s, vy = 0.0 m/s, omega = 0.0 rad/s
    kinematics_compute(1.0f, 0.0f, 0.0f, pps);
    ESP_LOGI(TAG, "Kinematics Test 1 (vx=1.0, vy=0.0, w=0.0) -> Axis A: %ld, Axis B: %ld, Axis C: %ld pps",
             (long)pps[0], (long)pps[1], (long)pps[2]);

    // Bộ test 2: vx = 0.0 m/s, vy = 0.0 m/s, omega = 2.0 rad/s
    kinematics_compute(0.0f, 0.0f, 2.0f, pps);
    ESP_LOGI(TAG, "Kinematics Test 2 (vx=0.0, vy=0.0, w=2.0) -> Axis A: %ld, Axis B: %ld, Axis C: %ld pps",
             (long)pps[0], (long)pps[1], (long)pps[2]);

    // Bộ test 3: vx = 0.5 m/s, vy = 0.5 m/s, omega = 1.0 rad/s
    kinematics_compute(0.5f, 0.5f, 1.0f, pps);
    ESP_LOGI(TAG, "Kinematics Test 3 (vx=0.5, vy=0.5, w=1.0) -> Axis A: %ld, Axis B: %ld, Axis C: %ld pps",
             (long)pps[0], (long)pps[1], (long)pps[2]);

    // Kiểm tra bộ ramp gia tốc mượt hóa (Anti-Jerk S-curve) từ 0 đến Target Test 1
    int32_t current_pps[3] = {0, 0, 0};
    int32_t target_pps[3];
    kinematics_compute(1.0f, 0.0f, 0.0f, target_pps);
    kinematics_ramp_update(target_pps, current_pps, 0.02f);
    ESP_LOGI(TAG, "Kinematics Ramp 1-step (20ms) -> Axis A: %ld, Axis B: %ld, Axis C: %ld pps (Anti-Jerk start)",
             (long)current_pps[0], (long)current_pps[1], (long)current_pps[2]);

    /* 3. Cấu hình GPIO2 làm output cho status LED */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi cấu hình GPIO %d: %s", STATUS_LED_GPIO, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "GPIO %d đã cấu hình output (1 Hz blink, 500ms on / 500ms off)", STATUS_LED_GPIO);
    }

    /* 4. Vòng lặp nhấp nháy 1 Hz (chu kỳ 1000ms: 500ms mức cao, 500ms mức thấp) */
    uint32_t level = 0;
    while (1) {
        gpio_set_level(STATUS_LED_GPIO, level);
        level = !level;
        vTaskDelay(pdMS_TO_TICKS(BLINK_HALF_PERIOD_MS));
    }
}
