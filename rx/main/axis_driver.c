#include "axis_driver.h"
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"

static const char *TAG = "AXIS_DRIVER";

/* Độ phân giải đếm của MCPWM: 1 MHz tương ứng 1 tick = 1 microsecond */
#define MCPWM_BASE_RESOLUTION_HZ    1000000

typedef struct {
    int pul_gpio;
    int dir_gpio;
    int ena_gpio;
    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t oper;
    mcpwm_cmpr_handle_t cmpr;
    mcpwm_gen_handle_t gen;
    int32_t current_pps;
    bool dir_positive;
    bool is_running;
    bool is_enabled;
} axis_channel_t;

static axis_channel_t s_axes[NUM_AXES] = {
    {
        .pul_gpio = AXIS_A_PUL_GPIO,
        .dir_gpio = AXIS_A_DIR_GPIO,
        .ena_gpio = AXIS_A_ENA_GPIO,
        .current_pps = 0,
        .dir_positive = true,
        .is_running = false,
        .is_enabled = false,
    },
    {
        .pul_gpio = AXIS_B_PUL_GPIO,
        .dir_gpio = AXIS_B_DIR_GPIO,
        .ena_gpio = AXIS_B_ENA_GPIO,
        .current_pps = 0,
        .dir_positive = true,
        .is_running = false,
        .is_enabled = false,
    },
    {
        .pul_gpio = AXIS_C_PUL_GPIO,
        .dir_gpio = AXIS_C_DIR_GPIO,
        .ena_gpio = AXIS_C_ENA_GPIO,
        .current_pps = 0,
        .dir_positive = true,
        .is_running = false,
        .is_enabled = false,
    },
};

static bool s_driver_initialized = false;

esp_err_t axis_driver_init(void)
{
    if (s_driver_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Khởi tạo Axis Driver (MCPWM cho PUL, GPIO cho DIR/ENA)...");

    /* 1. Cấu hình GPIO cho các chân DIR và ENA */
    uint64_t pin_mask = 0;
    for (int i = 0; i < NUM_AXES; i++) {
        pin_mask |= (1ULL << s_axes[i].dir_gpio);
        pin_mask |= (1ULL << s_axes[i].ena_gpio);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi cấu hình GPIO DIR/ENA: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. Cấu hình 3 bộ MCPWM Timer, Operator, Comparator, Generator trong Group 0 */
    for (int i = 0; i < NUM_AXES; i++) {
        /* 2.1 Tạo Timer: cập nhật chu kỳ đồng bộ khi về 0 để băm xung mịn, không bị ngắt quãng */
        mcpwm_timer_config_t timer_conf = {
            .group_id = 0,
            .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
            .resolution_hz = MCPWM_BASE_RESOLUTION_HZ,
            .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
            .period_ticks = 1000, /* Mặc định 1000 us = 1 kHz */
            .flags = {
                .update_period_on_empty = 1,
            },
        };
        ret = mcpwm_new_timer(&timer_conf, &s_axes[i].timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi tạo MCPWM timer cho trục %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        /* 2.2 Tạo Operator */
        mcpwm_operator_config_t oper_conf = {
            .group_id = 0,
        };
        ret = mcpwm_new_operator(&oper_conf, &s_axes[i].oper);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi tạo MCPWM operator cho trục %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        ret = mcpwm_operator_connect_timer(s_axes[i].oper, s_axes[i].timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi kết nối operator với timer trục %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        /* 2.3 Tạo Comparator */
        mcpwm_comparator_config_t cmpr_conf = {
            .flags = {
                .update_cmp_on_tez = 1, /* Cập nhật ngưỡng so sánh khi counter = 0 */
            },
        };
        ret = mcpwm_new_comparator(s_axes[i].oper, &cmpr_conf, &s_axes[i].cmpr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi tạo MCPWM comparator cho trục %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        mcpwm_comparator_set_compare_value(s_axes[i].cmpr, 500);

        /* 2.4 Tạo Generator xuất ra chân PUL tương ứng */
        mcpwm_generator_config_t gen_conf = {
            .gen_gpio_num = s_axes[i].pul_gpio,
        };
        ret = mcpwm_new_generator(s_axes[i].oper, &gen_conf, &s_axes[i].gen);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi tạo MCPWM generator cho trục %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        /* Đặt hành động cho Generator: 0 -> HIGH, Compare -> LOW để tạo xung 50% duty cycle */
        mcpwm_generator_set_action_on_timer_event(
            s_axes[i].gen,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
        );
        mcpwm_generator_set_action_on_compare_event(
            s_axes[i].gen,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, s_axes[i].cmpr, MCPWM_GEN_ACTION_LOW)
        );

        /* Bắt đầu ở trạng thái dừng: force level LOW */
        mcpwm_generator_set_force_level(s_axes[i].gen, 0, true);

        /* Enable timer */
        ret = mcpwm_timer_enable(s_axes[i].timer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Lỗi enable MCPWM timer trục %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        s_axes[i].is_running = false;
        s_axes[i].current_pps = 0;
    }

    /* 3. Mặc định lúc boot: Cả 3 trục được kích hoạt ENA (có lực giữ) ngay lập tức theo mục 1.3.1 */
    axis_enable_all(true);

    s_driver_initialized = true;
    ESP_LOGI(TAG, "Axis Driver khởi tạo thành công. Cả 3 trục đã khóa lực giữ ENA.");
    return ESP_OK;
}

void axis_set_speed(int axis, int32_t pulses_per_sec, bool dir_positive)
{
    if (axis < 0 || axis >= NUM_AXES) {
        return;
    }

    axis_channel_t *ch = &s_axes[axis];

    /* Cập nhật chân chiều quay DIR */
    ch->dir_positive = dir_positive;
    gpio_set_level(ch->dir_gpio, dir_positive ? 1 : 0);

    int32_t abs_pps = labs(pulses_per_sec);
    ch->current_pps = abs_pps;

    if (abs_pps <= 0) {
        /* Dừng sinh xung: khóa chân PUL ở mức LOW */
        if (ch->is_running) {
            mcpwm_timer_start_stop(ch->timer, MCPWM_TIMER_STOP_EMPTY);
            mcpwm_generator_set_force_level(ch->gen, 0, true);
            ch->is_running = false;
        }
        return;
    }

    /*
     * Tính chu kỳ xung:
     * period_ticks = resolution_hz / abs_pps.
     * Ví dụ: 1000 pps -> 1000 ticks (1000 us = 1 ms).
     * Ngưỡng tối thiểu là 2 ticks (tương đương 500 kHz trần tối đa của opto driver).
     */
    uint32_t period_ticks = MCPWM_BASE_RESOLUTION_HZ / (uint32_t)abs_pps;
    if (period_ticks < 2) {
        period_ticks = 2;
    }
    uint32_t cmp_ticks = period_ticks / 2;
    if (cmp_ticks < 1) {
        cmp_ticks = 1;
    }

    /* Cập nhật chu kỳ và compare ticks mịn */
    mcpwm_timer_set_period(ch->timer, period_ticks);
    mcpwm_comparator_set_compare_value(ch->cmpr, cmp_ticks);

    /* Nhả force level và khởi chạy timer nếu đang dừng */
    if (!ch->is_running) {
        mcpwm_generator_set_force_level(ch->gen, -1, true);
        mcpwm_timer_start_stop(ch->timer, MCPWM_TIMER_START_NO_STOP);
        ch->is_running = true;
    }
}

void axis_stop_all(void)
{
    /*
     * Cắt xung lập tức cả 3 trục (E-stop).
     * TUYỆT ĐỐI KHÔNG thay đổi trạng thái chân ENA để bảo toàn lực giữ trục.
     */
    for (int i = 0; i < NUM_AXES; i++) {
        axis_channel_t *ch = &s_axes[i];
        if (ch->is_running) {
            mcpwm_timer_start_stop(ch->timer, MCPWM_TIMER_STOP_EMPTY);
            mcpwm_generator_set_force_level(ch->gen, 0, true);
            ch->is_running = false;
        }
        ch->current_pps = 0;
    }
    ESP_LOGW(TAG, "E-STOP: Cắt xung toàn bộ 3 trục ngay lập tức, duy trì lực giữ ENA.");
}

void axis_set_enable(int axis, bool enable)
{
    if (axis < 0 || axis >= NUM_AXES) {
        return;
    }

    axis_channel_t *ch = &s_axes[axis];
    ch->is_enabled = enable;

    /* Điều khiển mức logic ENA: LOW = Enable (kích hoạt opto sinking), HIGH = Disable */
    int level = enable ? ENA_ACTIVE_LEVEL : ENA_INACTIVE_LEVEL;
    gpio_set_level(ch->ena_gpio, level);
}

void axis_enable_all(bool enable)
{
    for (int i = 0; i < NUM_AXES; i++) {
        axis_set_enable(i, enable);
    }
}

bool axis_get_enable(int axis)
{
    if (axis < 0 || axis >= NUM_AXES) {
        return false;
    }
    return s_axes[axis].is_enabled;
}

int32_t axis_get_speed(int axis)
{
    if (axis < 0 || axis >= NUM_AXES) {
        return 0;
    }
    return s_axes[axis].current_pps;
}
