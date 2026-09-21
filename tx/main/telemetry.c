#include "telemetry.h"
#include "protocol.h"
#include "tx_server.h"
#include "tm1638.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_now.h"

static const char *TAG = "OMNI_TELEMETRY";

static bool s_link_ok = false;
static TickType_t s_last_telemetry_tick = 0;
static bool s_has_received_first = false;
static uint16_t s_last_seq_echo = 0;
static uint32_t s_rx_count = 0;
static uint32_t s_dropped_count = 0;
static portMUX_TYPE s_telemetry_mux = portMUX_INITIALIZER_UNLOCKED;

bool telemetry_process_packet(const uint8_t *data, size_t len)
{
    if (!data || len != sizeof(telemetry_packet_t)) {
        portENTER_CRITICAL(&s_telemetry_mux);
        s_dropped_count++;
        portEXIT_CRITICAL(&s_telemetry_mux);
        return false;
    }

    const telemetry_packet_t *pkt = (const telemetry_packet_t *)data;

    /* 1. Kiểm tra magic byte (0x5A) */
    if (pkt->magic != TELEMETRY_PACKET_MAGIC) {
        portENTER_CRITICAL(&s_telemetry_mux);
        s_dropped_count++;
        portEXIT_CRITICAL(&s_telemetry_mux);
        ESP_LOGW(TAG, "Loại bỏ gói telemetry: sai magic 0x%02X != 0x%02X",
                 pkt->magic, TELEMETRY_PACKET_MAGIC);
        return false;
    }

    /* 2. Kiểm tra mã CRC-16/CCITT-FALSE */
    uint16_t calc_crc = telemetry_packet_calc_crc(pkt);
    if (pkt->crc16 != calc_crc) {
        portENTER_CRITICAL(&s_telemetry_mux);
        s_dropped_count++;
        portEXIT_CRITICAL(&s_telemetry_mux);
        ESP_LOGW(TAG, "Loại bỏ gói telemetry: sai CRC 0x%04X != 0x%04X",
                 pkt->crc16, calc_crc);
        return false;
    }

    /* 3. Gói tin hợp lệ: cập nhật trạng thái liên kết */
    bool was_link_lost;
    portENTER_CRITICAL(&s_telemetry_mux);
    was_link_lost = !s_link_ok;
    s_link_ok = true;
    s_last_telemetry_tick = xTaskGetTickCount();
    s_has_received_first = true;
    s_last_seq_echo = pkt->seq_echo;
    s_rx_count++;
    portEXIT_CRITICAL(&s_telemetry_mux);

    /* Cập nhật LED 2 (tắt khi bình thường) và phát thông báo qua WebSocket */
    tm1638_set_led(1, false);

    if (was_link_lost) {
        ESP_LOGI(TAG, "Liên kết RX phục hồi: OK (seq_echo=%u, link_ok=%u)",
                 pkt->seq_echo, pkt->link_ok);
        tx_broadcast_ws_message("{\"rx_link\":\"OK\",\"link_ok\":true}");
    }

    return true;
}

void telemetry_check_timeout(void)
{
    bool transition_to_lost = false;

    portENTER_CRITICAL(&s_telemetry_mux);
    if (s_link_ok) {
        TickType_t now = xTaskGetTickCount();
        uint32_t elapsed_ms = pdTICKS_TO_MS(now - s_last_telemetry_tick);
        if (elapsed_ms > TELEMETRY_TIMEOUT_MS) {
            s_link_ok = false;
            transition_to_lost = true;
        }
    }
    portEXIT_CRITICAL(&s_telemetry_mux);

    if (transition_to_lost) {
        /* Bật LED đơn thứ 2 trên TM1638 (index 1) báo mất kết nối RX */
        tm1638_set_led(1, true);
        tx_broadcast_ws_message("{\"rx_link\":\"MẤT KẾT NỐI\",\"link_ok\":false}");
        ESP_LOGW(TAG, "MẤT KẾT NỐI RX! Quá %d ms không nhận được gói telemetry nào.",
                 TELEMETRY_TIMEOUT_MS);
    }
}

bool telemetry_is_link_ok(void)
{
    bool ok;
    portENTER_CRITICAL(&s_telemetry_mux);
    ok = s_link_ok;
    portEXIT_CRITICAL(&s_telemetry_mux);
    return ok;
}

uint16_t telemetry_get_last_seq_echo(void)
{
    uint16_t seq;
    portENTER_CRITICAL(&s_telemetry_mux);
    seq = s_last_seq_echo;
    portEXIT_CRITICAL(&s_telemetry_mux);
    return seq;
}

uint32_t telemetry_get_rx_count(void)
{
    uint32_t count;
    portENTER_CRITICAL(&s_telemetry_mux);
    count = s_rx_count;
    portEXIT_CRITICAL(&s_telemetry_mux);
    return count;
}

uint32_t telemetry_get_dropped_count(void)
{
    uint32_t count;
    portENTER_CRITICAL(&s_telemetry_mux);
    count = s_dropped_count;
    portEXIT_CRITICAL(&s_telemetry_mux);
    return count;
}

static void espnow_telemetry_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (!recv_info || !data || data_len <= 0) {
        return;
    }
    telemetry_process_packet(data, (size_t)data_len);
}

static void telemetry_task(void *arg)
{
    ESP_LOGI(TAG, "Task giám sát Telemetry bắt đầu hoạt động (50ms chu kỳ)...");
    int heartbeat_cnt = 0;

    while (1) {
        telemetry_check_timeout();

        /* Định kỳ 500ms (10 chu kỳ) gửi heartbeat trạng thái tới trình duyệt */
        if (++heartbeat_cnt >= 10) {
            heartbeat_cnt = 0;
            bool ok = telemetry_is_link_ok();
            tx_broadcast_ws_message(ok ? "{\"rx_link\":\"OK\",\"link_ok\":true}" :
                                         "{\"rx_link\":\"MẤT KẾT NỐI\",\"link_ok\":false}");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t telemetry_init(void)
{
    ESP_LOGI(TAG, "Khởi tạo hệ thống nhận Telemetry từ RX qua ESP-NOW...");

    /* Mặc định ban đầu khi chưa nhận gói nào: coi như chưa có kết nối, LED 2 sáng */
    portENTER_CRITICAL(&s_telemetry_mux);
    s_link_ok = false;
    s_has_received_first = false;
    portEXIT_CRITICAL(&s_telemetry_mux);

    tm1638_set_led(1, true);

    /* Đăng ký callback nhận dữ liệu ESP-NOW */
    esp_err_t err = esp_now_register_recv_cb(espnow_telemetry_recv_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi esp_now_register_recv_cb: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t ret = xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Không thể tạo telemetry_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Khởi tạo Telemetry hoàn tất thành công.");
    return ESP_OK;
}
