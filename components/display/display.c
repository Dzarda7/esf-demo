#include <stdio.h>
#include <sys/stat.h>
#include <esp_log.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lvgl_port.h"
#include "display.h"

// #define LCD_HOST  SPI3_HOST
// #define SCLK_PIN  6
// #define MOSI_PIN  5
// #define CS_PIN 7
// #define DC_PIN 4
// #define RST_PIN 8
// #define BK_LIGHT_PIN 9
// #define LCD_PIXEL_CLOCK_HZ 40000000

#define LCD_H_RES 240
#define LCD_V_RES 240

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define STATUS_CIRCLE_INIT_SIZE 250
#define STATUS_CIRCLE_END_SIZE 100

#define DISPLAY_ANIMATION_TIME 300

static lv_obj_t *selector = NULL;
static lv_obj_t *flasher = NULL;
static lv_obj_t *not_ready = NULL;
lv_obj_t *success = NULL;

static lv_obj_t *arc_progress = NULL;
static lv_obj_t *circle_success = NULL;
static lv_obj_t *check_sign = NULL;
static lv_obj_t *cross_sign = NULL;
static lv_obj_t *roller1 = NULL;
static lv_obj_t *arc_selector = NULL;
static lv_obj_t *label_not_ready = NULL;
static lv_obj_t *label_flasher = NULL;
static lv_obj_t *label_success = NULL;

static const char *TAG = "display";

static void not_ready_screen_init(void)
{
    lvgl_port_lock(0);

    not_ready = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(not_ready, lv_color_black(), LV_PART_MAIN);

    label_not_ready = lv_label_create(not_ready);
    lv_obj_set_style_text_color(label_not_ready, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_not_ready, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label_not_ready);

    lvgl_port_unlock();
}

static void selector_screen_init(void)
{
    lvgl_port_lock(0);

    selector = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(selector, lv_color_black(), LV_PART_MAIN);

    roller1 = lv_roller_create(selector);
    lv_roller_set_visible_row_count(roller1, 3);
    lv_obj_set_style_bg_color(roller1, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_border_color(roller1, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(roller1, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller1, lv_palette_main(LV_PALETTE_BLUE), LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller1, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_width(roller1, 180);
    lv_obj_center(roller1);

    arc_selector = lv_arc_create(selector);
    lv_arc_set_rotation(arc_selector, 270);
    lv_arc_set_bg_angles(arc_selector, 0, 360);
    lv_arc_set_value(arc_selector, 0);
    lv_obj_set_size(arc_selector, LCD_H_RES, LCD_V_RES);
    lv_obj_center(arc_selector);
    lv_obj_remove_style(arc_selector, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc_selector, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_selector, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_selector, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_selector, 5, LV_PART_INDICATOR);

    lv_obj_t *label_instruction = lv_label_create(selector);
    lv_obj_set_style_text_color(label_instruction, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_instruction, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label_instruction, LV_SYMBOL_RIGHT "\nRotate to select");
    lv_obj_align(label_instruction, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *label_arrow = lv_label_create(selector);
    lv_obj_set_style_text_color(label_arrow, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label_arrow, "Press\n" LV_SYMBOL_DOWN);
    lv_obj_align(label_arrow, LV_ALIGN_BOTTOM_MID, 0, -10);

    lvgl_port_unlock();
}

static void flasher_screen_init(void)
{
    lvgl_port_lock(0);

    flasher = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(flasher, lv_color_black(), LV_PART_MAIN);

    arc_progress = lv_arc_create(flasher);
    lv_arc_set_rotation(arc_progress, 270);
    lv_arc_set_bg_angles(arc_progress, 0, 360);
    lv_arc_set_range(arc_progress, 0, 100);
    lv_arc_set_value(arc_progress, 0);
    lv_obj_remove_style(arc_progress, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(arc_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc_progress, 100, 100);
    lv_obj_center(arc_progress);

    label_flasher = lv_label_create(flasher);
    lv_obj_set_style_text_color(label_flasher, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_flasher, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_align(label_flasher, LV_TEXT_ALIGN_CENTER, 0);

    lvgl_port_unlock();
}

static void success_screen_init(void)
{
    lvgl_port_lock(0);

    success = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(success, lv_color_black(), LV_PART_MAIN);

    static lv_style_t style_circle;
    lv_style_init(&style_circle);
    lv_style_set_bg_opa(&style_circle, LV_OPA_COVER);
    lv_style_set_radius(&style_circle, LV_RADIUS_CIRCLE);
    lv_style_set_border_width(&style_circle, 0);

    circle_success = lv_obj_create(success);
    lv_obj_add_style(circle_success, &style_circle, 0);
    lv_obj_center(circle_success);

    static lv_style_t style_check;
    lv_style_init(&style_check);
    lv_style_set_line_color(&style_check, lv_color_white());
    lv_style_set_line_width(&style_check, 10);
    lv_style_set_line_rounded(&style_check, true);

    static lv_point_precise_t check_points[] = { {0, 30}, {20, 50}, {50, 0} };
    check_sign = lv_line_create(success);
    lv_line_set_points(check_sign, check_points, 3);
    lv_obj_add_style(check_sign, &style_check, 0);
    lv_obj_center(check_sign);

    static lv_point_precise_t cross_points[] = { {0, 0}, {50, 50}, {25, 25}, {0, 50}, {50, 0} };
    cross_sign = lv_line_create(success);
    lv_line_set_points(cross_sign, cross_points, 5);
    lv_obj_add_style(cross_sign, &style_check, 0);
    lv_obj_center(cross_sign);

    label_success = lv_label_create(success);
    lv_obj_set_style_text_color(label_success, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_success, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *label_arrow = lv_label_create(success);
    lv_obj_set_style_text_color(label_arrow, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label_arrow, "Press\n" LV_SYMBOL_DOWN);
    lv_obj_align(label_arrow, LV_ALIGN_BOTTOM_MID, 0, -10);

    lvgl_port_unlock();
}

static void screens_init(void)
{
    not_ready_screen_init();
    selector_screen_init();
    flasher_screen_init();
    success_screen_init();
}

static void panel_init(display_config_t *display_config, esp_lcd_panel_io_handle_t *io_handle, esp_lcd_panel_handle_t *panel_handle)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << display_config->pin_num_bk_light,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(display_config->pin_num_bk_light, 1);


    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = display_config->pin_num_clk,
        .mosi_io_num = display_config->pin_num_mosi,
        .miso_io_num = display_config->pin_num_miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(display_config->host, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = display_config->pin_num_dc,
        .cs_gpio_num = display_config->pin_num_cs,
        .pclk_hz = display_config->spi_frequency,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_LOGI(TAG, "Install GC9A01 panel driver");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = display_config->pin_num_rst,
        .rgb_endian = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(display_config->host, &io_config, io_handle));
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(*io_handle, &panel_config, panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));
    esp_lcd_panel_invert_color(*panel_handle, true);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_handle, true));
}

static void lvgl_init(lvgl_config_t *lvgl_config, esp_lcd_panel_io_handle_t *io_handle, esp_lcd_panel_handle_t *panel_handle)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = lvgl_config->lvgl_task_priority,
        .task_stack = 6144,
        .task_affinity = lvgl_config->lvgl_task_core,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };

    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = *io_handle,
        .panel_handle = *panel_handle,
        .buffer_size = LCD_H_RES * 50,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        }
    };
    lvgl_port_add_disp(&disp_cfg);
}

void display_init(display_config_t *display_config, lvgl_config_t *lvgl_config)
{
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;

    panel_init(display_config, &io_handle, &panel_handle);
    lvgl_init(lvgl_config, &io_handle, &panel_handle);
    screens_init();
}

void flasher_screen_progress(uint8_t progress)
{
    lvgl_port_lock(0);
    lv_arc_set_value(arc_progress, progress);
    lvgl_port_unlock();
}

void flasher_screen_text(const char *text)
{
    lvgl_port_lock(0);
    lv_label_set_text(label_flasher, text);
    lvgl_port_unlock();
}

void selector_roller_change(selector_direction_t direction)
{
    lvgl_port_lock(0);
    if (direction == UP) {
        if (lv_roller_get_selected(roller1) != 0) {
            lv_roller_set_selected(roller1, lv_roller_get_selected(roller1) - 1, LV_ANIM_ON);
        }
    } else {
        lv_roller_set_selected(roller1, lv_roller_get_selected(roller1) + 1, LV_ANIM_ON);
    }
    lv_arc_set_value(arc_selector, lv_roller_get_selected(roller1));
    lv_obj_invalidate(arc_selector);
    lv_obj_invalidate(roller1);
    lvgl_port_unlock();
}

void selector_screen_get_selected(char *buf, uint32_t buf_size)
{
    lvgl_port_lock(0);
    lv_roller_get_selected_str(roller1, buf, buf_size);
    lvgl_port_unlock();
}

void selector_screen_set_options(char *options)
{
    lvgl_port_lock(0);
    lv_roller_set_options(roller1, options, LV_ROLLER_MODE_NORMAL);
    lv_arc_set_range(arc_selector, 0, lv_roller_get_option_cnt(roller1) - 1);
    lvgl_port_unlock();
}

void display_status(const char *text, screen_t screen)
{
    lv_color_t bg_color;
    lv_obj_t *sign;
    if (screen == FLASH_SUCCESS) {
        bg_color = lv_palette_main(LV_PALETTE_GREEN);
        sign = check_sign;
    } else {
        bg_color = lv_palette_main(LV_PALETTE_RED);
        sign = cross_sign;
    }
    lvgl_port_lock(0);
    lv_obj_set_size(circle_success, STATUS_CIRCLE_INIT_SIZE, STATUS_CIRCLE_INIT_SIZE);
    lv_obj_set_style_bg_color(circle_success, bg_color, LV_PART_MAIN);
    lv_obj_add_flag(check_sign, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cross_sign, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(label_success, "");
    lv_screen_load_anim(success, LV_SCR_LOAD_ANIM_FADE_ON, DISPLAY_ANIMATION_TIME, 0, 0);
    lvgl_port_unlock();
    uint16_t circle_success_size1 = STATUS_CIRCLE_INIT_SIZE;
    while (circle_success_size1 > STATUS_CIRCLE_END_SIZE) {
        circle_success_size1 -= 10;
        lvgl_port_lock(0);
        lv_obj_set_size(circle_success, circle_success_size1, circle_success_size1);
        lvgl_port_unlock();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    lvgl_port_lock(0);
    lv_label_set_text(label_success, text);
    lv_obj_remove_flag(sign, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

void screen_set(screen_t screen, const char *text)
{
    switch (screen) {
    case NOT_READY:
        lvgl_port_lock(0);
        lv_label_set_text(label_not_ready, text);
        lv_screen_load_anim(not_ready, LV_SCR_LOAD_ANIM_FADE_ON, DISPLAY_ANIMATION_TIME, 0, 0);
        lvgl_port_unlock();
        break;
        break;
    case SELECTOR:
        lvgl_port_lock(0);
        lv_screen_load_anim(selector, LV_SCR_LOAD_ANIM_FADE_ON, DISPLAY_ANIMATION_TIME, 0, 0);
        lvgl_port_unlock();
        break;
    case FLASHER:
        lvgl_port_lock(0);
        lv_label_set_text(label_flasher, text);
        lv_screen_load_anim(flasher, LV_SCR_LOAD_ANIM_FADE_ON, DISPLAY_ANIMATION_TIME, 0, 0);
        lvgl_port_unlock();
        break;
    case FLASH_SUCCESS:
        display_status(text, FLASH_SUCCESS);
        break;
    case FLASH_ERROR:
        display_status(text, FLASH_ERROR);
        break;
    default:
        break;
    }
}
