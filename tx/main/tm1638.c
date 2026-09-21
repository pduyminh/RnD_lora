#include "tm1638.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "tx_server.h"
#include "telemetry.h"

static const char *TAG = "TM1638";

static bool s_tm1638_estop = false;
static uint8_t s_digits[8] = {0};
static uint8_t s_leds[8] = {0};
static portMUX_TYPE s_tm1638_mux = portMUX_INITIALIZER_UNLOCKED;

/* Bảng mã 7 đoạn common cathode cho số 0-9 (LSB=a, b, c, d, e, f, g, dp=MSB) */
static const uint8_t DIGIT_TABLE[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
};

static void tm1638_delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

static void tm1638_write_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        gpio_set_level(TM1638_CLK_GPIO, 0);
        gpio_set_level(TM1638_DIO_GPIO, (data >> i) & 1);
        tm1638_delay_us(2);
        gpio_set_level(TM1638_CLK_GPIO, 1);
        tm1638_delay_us(2);
    }
}

static uint8_t tm1638_read_byte(void)
{
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        gpio_set_level(TM1638_CLK_GPIO, 0);
        tm1638_delay_us(2);
        if (gpio_get_level(TM1638_DIO_GPIO)) {
            val |= (1 << i);
        }
        gpio_set_level(TM1638_CLK_GPIO, 1);
        tm1638_delay_us(2);
    }
    return val;
}

static void tm1638_send_cmd(uint8_t cmd)
{
    gpio_set_level(TM1638_STB_GPIO, 0);
    tm1638_delay_us(1);
    tm1638_write_byte(cmd);
    gpio_set_level(TM1638_STB_GPIO, 1);
    tm1638_delay_us(1);
}

static void tm1638_flush(void)
{
    uint8_t raw_mem[16];
    portENTER_CRITICAL(&s_tm1638_mux);
    for (int i = 0; i < 8; i++) {
        raw_mem[2 * i] = s_digits[i];
        raw_mem[2 * i + 1] = s_leds[i] ? 1 : 0;
    }
    portEXIT_CRITICAL(&s_tm1638_mux);

    tm1638_send_cmd(TM1638_CMD_DATA_WRITE_AUTO);

    gpio_set_level(TM1638_STB_GPIO, 0);
    tm1638_delay_us(1);
    tm1638_write_byte(TM1638_CMD_ADDR_START);
    for (int i = 0; i < 16; i++) {
        tm1638_write_byte(raw_mem[i]);
    }
    gpio_set_level(TM1638_STB_GPIO, 1);
    tm1638_delay_us(1);
}

uint8_t tm1638_encode_digit(uint8_t digit)
{
    if (digit <= 9) {
        return DIGIT_TABLE[digit];
    }
    return 0x00;
}

uint8_t tm1638_encode_char(char c)
{
    if (c >= '0' && c <= '9') {
        return tm1638_encode_digit(c - '0');
    }
    switch (c) {
        case 'C': case 'c': return 0x39;
        case '-':           return 0x40;
        case 'E': case 'e': return 0x79;
        case 'S': case 's': return 0x6D;
        case 'T': case 't': return 0x78;
        case 'O': case 'o': return 0x3F;
        case 'P': case 'p': return 0x73;
        case ' ': default:  return 0x00;
    }
}

void tm1638_set_digit(uint8_t position, uint8_t segments)
{
    if (position < 8) {
        portENTER_CRITICAL(&s_tm1638_mux);
        s_digits[position] = segments;
        portEXIT_CRITICAL(&s_tm1638_mux);
    }
}

void tm1638_set_led(uint8_t index, bool on)
{
    if (index < 8) {
        portENTER_CRITICAL(&s_tm1638_mux);
        s_leds[index] = on ? 1 : 0;
        portEXIT_CRITICAL(&s_tm1638_mux);
    }
}

bool tm1638_get_estop(void)
{
    bool estop;
    portENTER_CRITICAL(&s_tm1638_mux);
    estop = s_tm1638_estop;
    portEXIT_CRITICAL(&s_tm1638_mux);
    return estop;
}

void tm1638_set_estop(bool estop)
{
    portENTER_CRITICAL(&s_tm1638_mux);
    s_tm1638_estop = estop;
    s_leds[0] = estop ? 1 : 0;
    portEXIT_CRITICAL(&s_tm1638_mux);
    tx_set_estop(estop ? 1 : 0);
}

uint8_t tm1638_read_keys(void)
{
    gpio_set_direction(TM1638_DIO_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(TM1638_STB_GPIO, 0);
    tm1638_delay_us(1);
    tm1638_write_byte(TM1638_CMD_DATA_READ_KEYS);
    
    gpio_set_direction(TM1638_DIO_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(TM1638_DIO_GPIO);
    tm1638_delay_us(2);

    uint8_t b[4];
    for (int i = 0; i < 4; i++) {
        b[i] = tm1638_read_byte();
    }
    gpio_set_level(TM1638_STB_GPIO, 1);
    gpio_set_direction(TM1638_DIO_GPIO, GPIO_MODE_OUTPUT);

    /* Ánh xạ 4 byte đọc về bitmap 8 phím S1 đến S8 */
    uint8_t keys = 0;
    if (b[0] & 0x01) keys |= (1 << 0);
    if (b[1] & 0x01) keys |= (1 << 1);
    if (b[2] & 0x01) keys |= (1 << 2);
    if (b[3] & 0x01) keys |= (1 << 3);
    if (b[0] & 0x10) keys |= (1 << 4);
    if (b[1] & 0x10) keys |= (1 << 5);
    if (b[2] & 0x10) keys |= (1 << 6);
    if (b[3] & 0x10) keys |= (1 << 7);

    return keys;
}

void tm1638_display_status(uint8_t client_count, int16_t vx_mm_s, bool estop)
{
    /* Vị trí 0-1: 'C' + số client kết nối (0-9) */
    tm1638_set_digit(0, tm1638_encode_char('C'));
    tm1638_set_digit(1, tm1638_encode_digit(client_count > 9 ? 9 : client_count));

    /* Vị trí 2: Khoảng trắng hoặc dấu cách phân tách */
    tm1638_set_digit(2, 0x00);

    /* Vị trí 3-7 (5 ký tự): Giá trị vx theo mm/s (-9999 đến 9999) */
    char buf[16];
    snprintf(buf, sizeof(buf), "%5d", (int)vx_mm_s);
    int len = (int)strlen(buf);
    int offset = len > 5 ? (len - 5) : 0;
    for (int i = 0; i < 5; i++) {
        char c = (offset + i < len) ? buf[offset + i] : ' ';
        tm1638_set_digit(3 + i, tm1638_encode_char(c));
    }

    /* LED 1 (index 0): Sáng khi estop, tắt khi bình thường */
    tm1638_set_led(0, estop);

    tm1638_flush();
}

static void tm1638_task(void *arg)
{
    ESP_LOGI(TAG, "Task TM1638 bắt đầu chạy (chu kỳ 40ms, 25Hz)...");
    uint8_t prev_keys = 0;

    /* Bật màn hình hiển thị với độ sáng tối đa */
    tm1638_send_cmd(TM1638_CMD_DISPLAY_ON | 0x07);

    while (1) {
        uint8_t keys = tm1638_read_keys();

        /* Phát hiện cạnh lên của Nút số 1 (bit 0): Bấm nút 1 để đảo trạng thái E-Stop */
        if ((keys & 0x01) && !(prev_keys & 0x01)) {
            bool new_estop = !tm1638_get_estop();
            tm1638_set_estop(new_estop);

            if (new_estop) {
                /* Gửi ngay lập tức gói E-Stop qua ESP-NOW không đợi chu kỳ 20ms */
                esp_err_t send_err = tx_send_packet_now();
                ESP_LOGW(TAG, "BẤM NÚT 1: KÍCH HOẠT E-STOP KHẨN CẤP! Gói phát ngay: %s",
                         send_err == ESP_OK ? "OK" : "ERR");
            } else {
                ESP_LOGI(TAG, "BẤM NÚT 1: HỦY BỎ E-STOP, trở về điều khiển bình thường.");
            }
        }
        prev_keys = keys;

        /* Cập nhật hiển thị số client và vận tốc vx */
        uint32_t ws_clients = tx_get_ws_client_count();
        int16_t cur_vx = tx_get_vx();
        bool is_estop = tm1638_get_estop();

        /* T07: Cập nhật LED đơn thứ 2 (index 1): sáng khi mất kết nối RX, tắt khi bình thường */
        tm1638_set_led(1, !telemetry_is_link_ok());

        tm1638_display_status((uint8_t)ws_clients, cur_vx, is_estop);

        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

esp_err_t tm1638_init(void)
{
    ESP_LOGI(TAG, "Khởi tạo GPIO cho module TM1638: STB=GPIO%d, CLK=GPIO%d, DIO=GPIO%d",
             TM1638_STB_GPIO, TM1638_CLK_GPIO, TM1638_DIO_GPIO);

    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << TM1638_STB_GPIO) | (1ULL << TM1638_CLK_GPIO) | (1ULL << TM1638_DIO_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&out_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi gpio_config TM1638: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level(TM1638_STB_GPIO, 1);
    gpio_set_level(TM1638_CLK_GPIO, 1);
    gpio_set_level(TM1638_DIO_GPIO, 1);

    /* Xóa sạch bộ nhớ hiển thị ban đầu */
    tm1638_flush();

    BaseType_t ret = xTaskCreate(tm1638_task, "tm1638_task", 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Không thể tạo tm1638_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Khởi tạo TM1638 hoàn tất thành công.");
    return ESP_OK;
}
