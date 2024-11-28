#pragma once

#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_host_device_t host;
    uint32_t spi_frequency;
    uint32_t pin_num_miso;
    uint32_t pin_num_mosi;
    uint32_t pin_num_clk;
    uint32_t pin_num_cs;
    uint32_t pin_num_dc;
    uint32_t pin_num_rst;
    uint32_t pin_num_bk_light;
} display_config_t;

typedef struct {
    uint8_t lvgl_task_core;
    uint8_t lvgl_task_priority;
} lvgl_config_t;


typedef enum {
    NOT_READY,
    SELECTOR,
    FLASHER,
    FLASH_SUCCESS,
    FLASH_ERROR,
} screen_t;

typedef enum {
    UP,
    DOWN,
} selector_direction_t;

void display_init(display_config_t *display_config, lvgl_config_t *lvgl_config);
void screen_set(screen_t screen, const char *text);
void selector_roller_change(selector_direction_t direction);
void selector_screen_get_selected(char *buf, uint32_t buf_size);
void selector_screen_set_options(char *options);
void flasher_screen_progress(uint8_t progress);

#ifdef __cplusplus
}
#endif