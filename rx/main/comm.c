#include "comm.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "kinematics.h"
#include "axis_driver.h"

static const char *TAG = "OMNI_COMM";

static uint32_t s_dropped_packet_count = 0;
static uint16_t s_last_seq = 0;
static bool s_has_first_seq = false;
static TickType_t s_last_valid_rx_tick = 0;

static bool s_failsafe_active = false;
static bool s_estop_active = false;

static int32_t s_target_pps[NUM_AXES] = {0, 0, 0};
static int32_t s_current_pps[NUM_AXES] = {0, 0, 0};

static uint8_t s_last_sender_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_has_sender_mac = false;

static portMUX_TYPE s_comm_mux = portMUX_INITIALIZER_UNLOCKED;

/* Forward declarations */
static void send_telemetry_echo(const uint8_t *dest_mac, uint16_t seq);

bool comm_process_packet(const uint8_t *src_mac, const uint8_t *data, size_t data_len)
{
    /* 1. Kiểm tra kích thước gói */
    if (data_len != sizeof(ctrl_packet_t)) {
        portENTER_CRITICAL(&s_comm_mux);
        s_dropped_packet_count++;
        portEXIT_CRITICAL(&s_comm_mux);
        ESP_LOGW(TAG, "Loại bỏ gói: sai kích thước (%u != %u). Tổng gói loại: %lu",
                 (unsigned int)data_len, (unsigned int)sizeof(ctrl_packet_t),
                 (unsigned long)s_dropped_packet_count);
        return false;
    }

    const ctrl_packet_t *pkt = (const ctrl_packet_t *)data;

    /* 2. Kiểm tra magic byte */
    if (pkt->magic != CTRL_PACKET_MAGIC) {
        portENTER_CRITICAL(&s_comm_mux);
        s_dropped_packet_count++;
        portEXIT_CRITICAL(&s_comm_mux);
        ESP_LOGW(TAG, "Loại bỏ gói: sai magic (0x%02X != 0x%02X). Tổng gói loại: %lu",
                 pkt->magic, CTRL_PACKET_MAGIC, (unsigned long)s_dropped_packet_count);
        return false;
    }

    /* 3. Kiểm tra CRC-16/CCITT-FALSE */
    size_t payload_len = sizeof(ctrl_packet_t) - sizeof(uint16_t);
    uint16_t calc_crc = crc16_ccitt_false((const uint8_t *)pkt, payload_len);
    if (calc_crc != pkt->crc16) {
        portENTER_CRITICAL(&s_comm_mux);
        s_dropped_packet_count++;
        portEXIT_CRITICAL(&s_comm_mux);
        ESP_LOGW(TAG, "Loại bỏ gói: sai CRC (tính: 0x%04X != gói: 0x%04X). Tổng gói loại: %lu",
                 calc_crc, pkt->crc16, (unsigned long)s_dropped_packet_count);
        return false;
    }

    /* 4. Kiểm tra Sequence number (chống gói cũ/lặp) */
    portENTER_CRITICAL(&s_comm_mux);
    if (s_has_first_seq) {
        int16_t diff = (int16_t)(pkt->seq - s_last_seq);
        if (diff <= 0) {
            s_dropped_packet_count++;
            portEXIT_CRITICAL(&s_comm_mux);
            ESP_LOGW(TAG, "Loại bỏ gói: seq cũ/lặp (nhận %u <= gần nhất %u). Tổng gói loại: %lu",
                     pkt->seq, s_last_seq, (unsigned long)s_dropped_packet_count);
            return false;
        }
    }
    s_last_seq = pkt->seq;
    s_has_first_seq = true;
    s_last_valid_rx_tick = xTaskGetTickCount();
    s_failsafe_active = false;
    portEXIT_CRITICAL(&s_comm_mux);

    /* Lưu MAC nguồn để gửi phản hồi telemetry */
    if (src_mac) {
        memcpy(s_last_sender_mac, src_mac, ESP_NOW_ETH_ALEN);
        s_has_sender_mac = true;
    }

    /* 5. Xử lý E-Stop khẩn cấp */
    if (pkt->estop == 1) {
        s_estop_active = true;
        /*
         * Cắt xung ngay lập tức cả 3 trục không qua ramp.
         * TUYỆT ĐỐI KHÔNG tắt ENA (bảo toàn lực giữ trục).
         */
        axis_stop_all();
        s_target_pps[0] = 0;
        s_target_pps[1] = 0;
        s_target_pps[2] = 0;
        s_current_pps[0] = 0;
        s_current_pps[1] = 0;
        s_current_pps[2] = 0;
        ESP_LOGW(TAG, "E-STOP NHẬN ĐƯỢC: Đã cắt xung khẩn cấp, duy trì mô-men giữ ENA.");
    } else {
        s_estop_active = false;
        /* Quy đổi mm/s sang m/s và mrad/s sang rad/s */
        float vx = (float)pkt->vx_mm_s / 1000.0f;
        float vy = (float)pkt->vy_mm_s / 1000.0f;
        float omega = (float)pkt->omega_mrad_s / 1000.0f;

        /* Tính toán động học tam giác (kinematics) */
        kinematics_compute(vx, vy, omega, s_target_pps);
    }

    /* 6. Gửi telemetry phản hồi về MAC nguồn với seq_echo và link_ok = 1 */
    if (s_has_sender_mac) {
        send_telemetry_echo(s_last_sender_mac, pkt->seq);
    }

    return true;
}

static void send_telemetry_echo(const uint8_t *dest_mac, uint16_t seq)
{
    /* Đảm bảo peer đã được thêm vào danh sách ESP-NOW */
    if (!esp_now_is_peer_exist(dest_mac)) {
        esp_now_peer_info_t peer = {
            .channel = 1,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, dest_mac, ESP_NOW_ETH_ALEN);
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi thêm ESP-NOW peer: %s", esp_err_to_name(err));
            return;
        }
    }

    telemetry_packet_t telem = {
        .magic = TELEMETRY_PACKET_MAGIC,
        .seq_echo = seq,
        .driver_fault_bitmap = 0x00,
        .link_ok = 1,
        .reserved = 0x00,
        .crc16 = 0x0000,
    };

    size_t payload_len = sizeof(telemetry_packet_t) - sizeof(uint16_t);
    telem.crc16 = crc16_ccitt_false((const uint8_t *)&telem, payload_len);

    esp_err_t send_res = esp_now_send(dest_mac, (const uint8_t *)&telem, sizeof(telem));
    if (send_res != ESP_OK) {
        ESP_LOGD(TAG, "Gửi telemetry thất bại: %s", esp_err_to_name(send_res));
    }
}

void comm_pipeline_step(void)
{
    TickType_t now = xTaskGetTickCount();
    uint32_t elapsed_ms = pdTICKS_TO_MS(now - s_last_valid_rx_tick);

    /* 1. Kiểm tra điều kiện Failsafe 300ms */
    if (s_has_first_seq && (elapsed_ms > FAILSAFE_TIMEOUT_MS)) {
        if (!s_failsafe_active) {
            s_failsafe_active = true;
            /* Yêu cầu chính xác: in dòng "FAILSAFE TRIGGERED" ra UART */
            ESP_LOGW(TAG, "FAILSAFE TRIGGERED");
            ESP_LOGW(TAG, "FAILSAFE: Không nhận được gói hợp lệ trong %lu ms (> 300ms). Ramp dừng êm.",
                     (unsigned long)elapsed_ms);
        }
        /* Đặt mục tiêu vận tốc về 0 để ramp giảm tốc êm ái */
        s_target_pps[0] = 0;
        s_target_pps[1] = 0;
        s_target_pps[2] = 0;
    }

    /* 2. Nếu đang trong trạng thái E-Stop: giữ tốc độ = 0 */
    if (s_estop_active) {
        return;
    }

    /* 3. Cập nhật ramp gia tốc mượt hóa (Anti-Jerk S-curve) chu kỳ 20ms */
    kinematics_ramp_update(s_target_pps, s_current_pps, 0.02f);

    /* 4. Điều khiển xung và chiều ra 3 trục qua MCPWM */
    for (int i = 0; i < NUM_AXES; i++) {
        int32_t pps = s_current_pps[i];
        bool dir_pos = (pps >= 0);
        axis_set_speed(i, pps, dir_pos);
    }
}

/* Callback nhận gói tin từ ESP-NOW */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (!recv_info || !data || data_len <= 0) {
        return;
    }
    comm_process_packet(recv_info->src_addr, data, (size_t)data_len);
}

static void pipeline_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(PIPELINE_PERIOD_MS);
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        comm_pipeline_step();
        vTaskDelayUntil(&last_wake_time, period);
    }
}

esp_err_t comm_init(void)
{
    ESP_LOGI(TAG, "Khởi tạo ESP-NOW RX & Pipeline Task...");

    /* 1. Khởi tạo NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi nvs_flash_init: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. Khởi tạo Netif và Event loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Lỗi esp_event_loop_create_default: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 3. Khởi tạo WiFi Station mode */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    /* 4. Khởi tạo ESP-NOW */
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi esp_now_init: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    /* 5. Tạo FreeRTOS task chu kỳ 20ms (50Hz) để chạy pipeline điều khiển */
    BaseType_t task_ret = xTaskCreate(pipeline_task, "comm_pipe_task", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Lỗi tạo pipeline task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ESP-NOW RX & Pipeline Task đã khởi tạo thành công (chu kỳ 20ms / 50Hz).");
    return ESP_OK;
}

uint32_t comm_get_dropped_packet_count(void)
{
    return s_dropped_packet_count;
}

uint16_t comm_get_last_seq(void)
{
    return s_last_seq;
}

bool comm_is_failsafe_active(void)
{
    return s_failsafe_active;
}

bool comm_is_estop_active(void)
{
    return s_estop_active;
}
