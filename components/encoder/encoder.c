#include <stdio.h>
#include "encoder.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "iot_button.h"
#include "freertos/queue.h"

#define PCNT_HIGH_LIMIT 4
#define PCNT_LOW_LIMIT -4

static const char *TAG = "encoder";
QueueHandle_t encoder_state_queue = NULL;

static bool example_pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    encoder_t enc;
    if (edata->watch_point_value == PCNT_HIGH_LIMIT) {
        enc = ENCODER_MOVE_RIGHT;
    } else if (edata->watch_point_value == PCNT_LOW_LIMIT) {
        enc = ENCODER_MOVE_LEFT;
    }
    xQueueOverwriteFromISR(encoder_state_queue, &enc, &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

static void dial_init(uint32_t encoder_a_pin, uint32_t encoder_b_pin)
{
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 2000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = encoder_a_pin,
        .level_gpio_num = encoder_b_pin,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = encoder_b_pin,
        .level_gpio_num = encoder_a_pin,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    int watch_points[] = {PCNT_LOW_LIMIT, PCNT_HIGH_LIMIT};
    for (size_t i = 0; i < sizeof(watch_points) / sizeof(watch_points[0]); i++) {
        ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, watch_points[i]));
    }
    pcnt_event_callbacks_t cbs = {
        .on_reach = example_pcnt_on_reach,
    };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, NULL));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

static void button_single_click_cb(void *arg, void *usr_data)
{
    encoder_t enc = ENCODER_CLICKED;
    xQueueOverwriteFromISR(encoder_state_queue, &enc, NULL);
}

static esp_err_t button_init(uint32_t encoder_btn_pin)
{
    button_config_t btn_config = {
        .type = BUTTON_TYPE_GPIO,
        .short_press_time = 50,
        .gpio_button_config = {
            .gpio_num = encoder_btn_pin,
            .active_level = 0,
        },
    };
    button_handle_t btn_handle = iot_button_create(&btn_config);
    if (btn_handle == NULL) {
        ESP_LOGE(TAG, "create button failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "create button success");

    ESP_ERROR_CHECK(iot_button_register_cb(btn_handle, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL));
    return ESP_OK;
}

esp_err_t encoder_init(encoder_config_t *config)
{
    encoder_state_queue = xQueueCreate(1, sizeof(encoder_t));
    dial_init(config->encoder_a_pin, config->encoder_b_pin);
    ESP_ERROR_CHECK(button_init(config->encoder_btn_pin));
    return ESP_OK;
}

encoder_t encoder_get_value(void)
{
    encoder_t enc;
    if (xQueueReceive(encoder_state_queue, &enc, 0) == pdTRUE) {
        return enc;
    }

    return ENCODER_NO_MOVE;
}