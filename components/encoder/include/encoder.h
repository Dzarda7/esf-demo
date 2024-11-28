#pragma once

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t encoder_a_pin;
    uint32_t encoder_b_pin;
    uint32_t encoder_btn_pin;
} encoder_config_t;

typedef enum {
    ENCODER_NO_MOVE = 0,
    ENCODER_CLICKED = 1,
    ENCODER_MOVE_LEFT = 2,
    ENCODER_MOVE_RIGHT = 3,
} encoder_t;

esp_err_t encoder_init(encoder_config_t *config);
encoder_t encoder_get_value(void);

#ifdef __cplusplus
}
#endif