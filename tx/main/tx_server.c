#include "tx_server.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

static const char *TAG = "OMNI_TX_SERVER";

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint16_t s_seq = 0;
static int16_t s_vx_mm_s = 0;
static int16_t s_vy_mm_s = 0;
static int16_t s_omega_mrad_s = 0;
static uint8_t s_estop = 0;

static uint32_t s_ws_client_count = 0;
static TickType_t s_last_ws_rx_tick = 0;
static portMUX_TYPE s_tx_mux = portMUX_INITIALIZER_UNLOCKED;

static httpd_handle_t s_http_server = NULL;

/* HTML trang điều khiển với Virtual Joystick 2 trục, thanh xoay Omega và nút E-Stop */
static const char INDEX_HTML[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='utf-8'/>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'/>"
"<title>Omni Robot Controller</title>"
"<style>"
"body { font-family: sans-serif; background: #1a1a1a; color: #eee; text-align: center; margin: 0; padding: 10px; }"
"h2 { margin: 10px 0; color: #00d2ff; }"
"#status { margin: 5px 0; font-size: 14px; color: #bbb; }"
".connected { color: #00ff66 !important; }"
".disconnected { color: #ff3333 !important; }"
"#joystick-container {"
"  position: relative; width: 240px; height: 240px; margin: 20px auto;"
"  background: #2a2a2a; border-radius: 50%; border: 3px solid #00d2ff;"
"  touch-action: none;"
"}"
"#joystick-handle {"
"  position: absolute; width: 80px; height: 80px; background: #00d2ff;"
"  border-radius: 50%; top: 80px; left: 80px; box-shadow: 0 0 15px rgba(0,210,255,0.6);"
"}"
".slider-container { width: 280px; margin: 20px auto; }"
".slider-container label { display: block; margin-bottom: 5px; font-weight: bold; }"
"input[type=range] { width: 100%; height: 10px; border-radius: 5px; background: #333; outline: none; }"
"#estop-btn {"
"  background: #ff2222; color: #fff; font-size: 20px; font-weight: bold;"
"  padding: 15px 40px; border: none; border-radius: 8px; cursor: pointer;"
"  margin-top: 15px; box-shadow: 0 0 15px rgba(255,34,34,0.6);"
"}"
"#estop-btn:active { background: #cc0000; }"
"</style>"
"</head>"
"<body>"
"<h2>OMNI ROBOT TX</h2>"
"<div id='status' class='disconnected'>WebSocket: Disconnected</div>"
"<div id='joystick-container'>"
"  <div id='joystick-handle'></div>"
"</div>"
"<div class='slider-container'>"
"  <label id='omega-label'>Xoay Omega: 0 mrad/s</label>"
"  <input type='range' id='omega-slider' min='-2000' max='2000' value='0' step='50'/>"
"</div>"
"<button id='estop-btn'>EMERGENCY STOP</button>"
"<script>"
"var ws = null;"
"var statusEl = document.getElementById('status');"
"var container = document.getElementById('joystick-container');"
"var handle = document.getElementById('joystick-handle');"
"var omegaSlider = document.getElementById('omega-slider');"
"var omegaLabel = document.getElementById('omega-label');"
"var estopBtn = document.getElementById('estop-btn');"
"var vx = 0, vy = 0, omega = 0, estop = 0;"
"var isDragging = false;"
"var rect = container.getBoundingClientRect();"
"var radius = 120, maxDist = 80;"
"function connectWS() {"
"  var wsUri = 'ws://' + location.host + '/ws';"
"  ws = new WebSocket(wsUri);"
"  ws.onopen = function() { statusEl.textContent = 'WebSocket: Connected'; statusEl.className = 'connected'; };"
"  ws.onclose = function() { statusEl.textContent = 'WebSocket: Disconnected'; statusEl.className = 'disconnected'; setTimeout(connectWS, 1000); };"
"}"
"connectWS();"
"function sendCommand() {"
"  if (ws && ws.readyState === WebSocket.OPEN) {"
"    ws.send(JSON.stringify({vx: Math.round(vx), vy: Math.round(vy), omega: Math.round(omega), estop: estop}));"
"  }"
"}"
"setInterval(sendCommand, 50);"
"function handleMove(clientX, clientY) {"
"  rect = container.getBoundingClientRect();"
"  var cx = rect.left + radius, cy = rect.top + radius;"
"  var dx = clientX - cx, dy = clientY - cy;"
"  var dist = Math.hypot(dx, dy);"
"  if (dist > maxDist) { dx = (dx / dist) * maxDist; dy = (dy / dist) * maxDist; }"
"  handle.style.left = (radius - 40 + dx) + 'px';"
"  handle.style.top = (radius - 40 + dy) + 'px';"
"  vx = (dx / maxDist) * 1000;"
"  vy = (-dy / maxDist) * 1000;"
"}"
"container.addEventListener('pointerdown', function(e) { isDragging = true; handleMove(e.clientX, e.clientY); });"
"window.addEventListener('pointermove', function(e) { if (isDragging) handleMove(e.clientX, e.clientY); });"
"window.addEventListener('pointerup', function(e) {"
"  if (isDragging) {"
"    isDragging = false;"
"    handle.style.left = '80px'; handle.style.top = '80px';"
"    vx = 0; vy = 0;"
"    sendCommand();"
"  }"
"});"
"omegaSlider.addEventListener('input', function(e) {"
"  omega = parseInt(e.target.value);"
"  omegaLabel.textContent = 'Xoay Omega: ' + omega + ' mrad/s';"
"});"
"omegaSlider.addEventListener('pointerup', function(e) {"
"  omegaSlider.value = 0; omega = 0;"
"  omegaLabel.textContent = 'Xoay Omega: 0 mrad/s';"
"  sendCommand();"
"});"
"estopBtn.addEventListener('click', function() {"
"  estop = 1; vx = 0; vy = 0; omega = 0;"
"  sendCommand();"
"  setTimeout(function() { estop = 0; }, 200);"
"});"
"</script>"
"</body>"
"</html>";

static esp_err_t http_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket client kết nối mới (Handshake OK)");
        portENTER_CRITICAL(&s_tx_mux);
        s_ws_client_count++;
        s_last_ws_rx_tick = xTaskGetTickCount();
        portEXIT_CRITICAL(&s_tx_mux);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t buf[128];
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = buf;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf) - 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi nhận WebSocket frame: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len > 0) {
        buf[ws_pkt.len] = '\0';
        int vx = 0, vy = 0, omega = 0, estop = 0;
        /* Phân tích chuỗi JSON đơn giản {"vx":%d,"vy":%d,"omega":%d,"estop":%d} */
        char *p_vx = strstr((char *)buf, "\"vx\":");
        char *p_vy = strstr((char *)buf, "\"vy\":");
        char *p_om = strstr((char *)buf, "\"omega\":");
        char *p_es = strstr((char *)buf, "\"estop\":");

        if (p_vx) sscanf(p_vx + 5, "%d", &vx);
        if (p_vy) sscanf(p_vy + 5, "%d", &vy);
        if (p_om) sscanf(p_om + 8, "%d", &omega);
        if (p_es) sscanf(p_es + 8, "%d", &estop);

        tx_set_motion_command((int16_t)vx, (int16_t)vy, (int16_t)omega, (uint8_t)estop);

        portENTER_CRITICAL(&s_tx_mux);
        s_last_ws_rx_tick = xTaskGetTickCount();
        portEXIT_CRITICAL(&s_tx_mux);
    } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        portENTER_CRITICAL(&s_tx_mux);
        if (s_ws_client_count > 0) {
            s_ws_client_count--;
        }
        portEXIT_CRITICAL(&s_tx_mux);
        ESP_LOGI(TAG, "WebSocket client ngắt kết nối (Close frame)");
    }

    return ESP_OK;
}

void tx_set_motion_command(int16_t vx_mm_s, int16_t vy_mm_s, int16_t omega_mrad_s, uint8_t estop)
{
    portENTER_CRITICAL(&s_tx_mux);
    s_vx_mm_s = vx_mm_s;
    s_vy_mm_s = vy_mm_s;
    s_omega_mrad_s = omega_mrad_s;
    s_estop = estop;
    portEXIT_CRITICAL(&s_tx_mux);
}

uint32_t tx_get_ws_client_count(void)
{
    uint32_t count;
    portENTER_CRITICAL(&s_tx_mux);
    count = s_ws_client_count;
    portEXIT_CRITICAL(&s_tx_mux);
    return count;
}

uint16_t tx_get_current_seq(void)
{
    uint16_t seq;
    portENTER_CRITICAL(&s_tx_mux);
    seq = s_seq;
    portEXIT_CRITICAL(&s_tx_mux);
    return seq;
}

void tx_build_ctrl_packet(ctrl_packet_t *pkt)
{
    if (!pkt) return;

    portENTER_CRITICAL(&s_tx_mux);
    s_seq++;
    pkt->magic = CTRL_PACKET_MAGIC;
    pkt->seq = s_seq;

    /*
     * Tiêu chí: Khi không có client nào kết nối WebSocket (hoặc quá 500ms không nhận gói),
     * TX gửi gói với vx=vy=omega=0 để đảm bảo RX không nhận lệnh rác.
     */
    TickType_t now = xTaskGetTickCount();
    bool client_active = (s_ws_client_count > 0) &&
                         (pdTICKS_TO_MS(now - s_last_ws_rx_tick) <= 500);

    if (client_active) {
        pkt->vx_mm_s = s_vx_mm_s;
        pkt->vy_mm_s = s_vy_mm_s;
        pkt->omega_mrad_s = s_omega_mrad_s;
        pkt->estop = s_estop;
    } else {
        pkt->vx_mm_s = 0;
        pkt->vy_mm_s = 0;
        pkt->omega_mrad_s = 0;
        pkt->estop = s_estop;
    }
    portEXIT_CRITICAL(&s_tx_mux);

    pkt->crc16 = ctrl_packet_calc_crc(pkt);
}

static void espnow_tx_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(TX_ESPNOW_PERIOD_MS);
    TickType_t last_wake_time = xTaskGetTickCount();
    ctrl_packet_t pkt;

    while (1) {
        /* Đóng gói và gửi đúng chu kỳ 20ms (50 Hz ± 2ms) */
        tx_build_ctrl_packet(&pkt);

        esp_err_t err = esp_now_send(s_broadcast_mac, (const uint8_t *)&pkt, sizeof(pkt));
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "esp_now_send thất bại: %s", esp_err_to_name(err));
        }

        vTaskDelayUntil(&last_wake_time, period);
    }
}

esp_err_t tx_server_init(void)
{
    ESP_LOGI(TAG, "Khởi tạo TX Server (SoftAP, HTTP Web Server, WebSocket & 50Hz ESP-NOW)...");

    /* 1. NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi nvs_flash_init: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. Netif & Event Loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Lỗi esp_event_loop_create_default: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_ap();

    /* 3. Khởi tạo WiFi ở chế độ SoftAP */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = TX_AP_SSID,
            .ssid_len = strlen(TX_AP_SSID),
            .channel = TX_AP_CHANNEL,
            .password = TX_AP_PASSWORD,
            .max_connection = TX_AP_MAX_CONN,
            .authmode = (strlen(TX_AP_PASSWORD) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi SoftAP đã khởi động. SSID: %s (Channel %d)", TX_AP_SSID, TX_AP_CHANNEL);

    /* 4. Khởi tạo ESP-NOW & Broadcast Peer */
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi esp_now_init: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_now_peer_info_t peer = {
        .channel = TX_AP_CHANNEL,
        .ifidx = WIFI_IF_AP,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi thêm broadcast peer: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 5. Khởi tạo HTTP Web Server và đăng ký WebSocket endpoint */
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.max_open_sockets = 7;
    http_config.lru_purge_enable = true;

    ret = httpd_start(&s_http_server, &http_config);
    if (ret == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(s_http_server, &uri_get);

        httpd_uri_t uri_ws = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws_handler,
            .user_ctx = NULL,
            .is_websocket = true,
        };
        httpd_register_uri_handler(s_http_server, &uri_ws);
        ESP_LOGI(TAG, "HTTP Server & WebSocket đã sẵn sàng trên cổng 80.");
    } else {
        ESP_LOGE(TAG, "Lỗi khởi động HTTP Server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 6. Tạo FreeRTOS Task phát ESP-NOW 50 Hz độc lập */
    BaseType_t task_ret = xTaskCreate(espnow_tx_task, "espnow_tx_task", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Lỗi tạo espnow_tx_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TX Server khởi tạo thành công (ESP-NOW 50Hz chu kỳ 20ms).");
    return ESP_OK;
}
