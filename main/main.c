#include <stdio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <string.h>
#include "encoder.h"
#include "display.h"
#include "card_reader.h"
#include "esp32_usb_cdc_acm_port.h"
#include "esp_loader.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// For esp8266, esp32, esp32s2
#define BOOTLOADER_ADDRESS_V0       0x1000
// For esp32s3 and later chips
#define BOOTLOADER_ADDRESS_V1       0x0
#define PARTITION_ADDRESS           0x8000
#define APPLICATION_ADDRESS         0x10000

#define BOOTLOADER_NAME         "bootloader.bin"
#define APP_NAME                "app.bin"
#define PARTITION_TABLE_NAME    "partition-table.bin"

#define ESPRESSIF_VID 0x303a
#define ESP_SERIAL_JTAG_PID 0x1001

static const char *TAG = "ESF_DEMO";
static TaskHandle_t usbConnectTaskHandle = NULL;

typedef struct {
    FILE *file;
    int32_t size;
    uint32_t address;
} flasher_image_t;

typedef struct {
    bool device_connected;
    bool card_mounted;
} device_state_t;

static flasher_image_t flash_file_prepare(const char *path, uint32_t address)
{
    flasher_image_t file = {0};
    struct stat st;
    if (stat(path, &st) == 0) {
        file.size = st.st_size;
        file.address = address;
        file.file = fopen(path, "rb");
        if (!file.file) {
            ESP_LOGE(TAG, "Failed to open file %s", path);
        }
    } else {
        ESP_LOGE(TAG, "Failed to stat file %s", path);
    }
    return file;
}

static esp_loader_error_t flash_binary(FILE *bin_file, size_t size, size_t address, const char *file_name)
{
    esp_loader_error_t err;
    static uint8_t payload[1024];

    ESP_LOGI(TAG, "Erasing flash,\nplease wait...");
    screen_set(FLASHER, "Erasing flash,\nplease wait...");
    err = esp_loader_flash_start(address, size, sizeof(payload));
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGI(TAG, "Failed to erase flash");
        return err;
    }

    char text[64];
    snprintf(text, sizeof(text), "Flashing...\n %s", file_name);
    ESP_LOGI(TAG, "Flashing %s", file_name);
    screen_set(FLASHER, text);
    size_t written = 0;
    while (written < size) {
        size_t read_bytes = fread(payload, sizeof(uint8_t), sizeof(payload), bin_file);
        err = esp_loader_flash_write(payload, read_bytes);
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }

        written += read_bytes;

        uint8_t progress = (uint8_t)(((float)written / size) * 100);
        flasher_screen_progress(progress);
        vTaskDelay(1 / portTICK_PERIOD_MS); // Yield to watchdog reset
    };
    err = esp_loader_flash_verify();
    if (err != ESP_LOADER_SUCCESS) {
        return err;
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t flash_process(const char *proj_name)
{
    esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&connect_config) == ESP_LOADER_SUCCESS) {
        esp_loader_error_t err;
        uint16_t path_len;
        flasher_image_t image;
        char *path;

        path_len = strlen(MOUNT_POINT) + strlen(proj_name) + strlen(BOOTLOADER_NAME) + 2 + 1; // +2 for slashes, +1 for null terminator
        path = (char *)malloc(path_len);
        snprintf(path, path_len, "%s/%s/%s", MOUNT_POINT, proj_name, BOOTLOADER_NAME);
        image = flash_file_prepare(path, BOOTLOADER_ADDRESS_V1);
        if (image.file == NULL) {
            free(path);
            return ESP_LOADER_ERROR_FAIL;
        }
        err = flash_binary(image.file, image.size, image.address, BOOTLOADER_NAME);
        fclose(image.file);
        if (err != ESP_LOADER_SUCCESS) {
            if (path != NULL) {
                free(path);
            }
            return err;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);

        path_len = strlen(MOUNT_POINT) + strlen(proj_name) + strlen(PARTITION_TABLE_NAME) + 2 + 1; // +2 for slashes, +1 for null terminator
        path = (char *)realloc(path, path_len);
        snprintf(path, path_len, "%s/%s/%s", MOUNT_POINT, proj_name, PARTITION_TABLE_NAME);
        image = flash_file_prepare(path, PARTITION_ADDRESS);
        if (image.file == NULL) {
            free(path);
            return ESP_LOADER_ERROR_FAIL;
        }
        err = flash_binary(image.file, image.size, image.address, PARTITION_TABLE_NAME);
        fclose(image.file);
        if (err != ESP_LOADER_SUCCESS) {
            if (path != NULL) {
                free(path);
            }
            return err;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);

        path_len = strlen(MOUNT_POINT) + strlen(proj_name) + strlen(APP_NAME) + 2 + 1; // +2 for slashes, +1 for null terminator
        path = (char *)realloc(path, path_len);
        snprintf(path, path_len, "%s/%s/%s", MOUNT_POINT, proj_name, APP_NAME);
        image = flash_file_prepare(path, APPLICATION_ADDRESS);
        if (image.file == NULL) {
            free(path);
            return ESP_LOADER_ERROR_FAIL;
        }
        err = flash_binary(image.file, image.size, image.address, APP_NAME);
        fclose(image.file);
        if (err != ESP_LOADER_SUCCESS) {
            if (path != NULL) {
                free(path);
            }
            return err;
        }
        free(path);
        esp_loader_reset_target();
        return ESP_LOADER_SUCCESS;
    }
    else {
        ESP_LOGE(TAG, "Failed to connect to the device");
    }
    return ESP_LOADER_ERROR_FAIL;
}

static void usb_lib_task(void *arg)
{
    while (1) {
        /* Start handling system events */
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB: All devices freed");
            /* Continue handling USB events to allow device reconnection */
        }
    }
}

static void device_disconnected_callback(void)
{
    xTaskResumeFromISR(usbConnectTaskHandle);
}

static void usb_connect_task(void *arg)
{

    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    BaseType_t task_created = xTaskCreatePinnedToCore(usb_lib_task,
                              "usb_lib",
                              4096,
                              xTaskGetCurrentTaskHandle(),
                              20,
                              NULL,
                              1);
    assert(task_created == pdTRUE);


    cdc_acm_host_driver_config_t cdc_acm_driver_config = {
        .driver_task_stack_size = 4096,
        .driver_task_priority = 19,
        .xCoreID = 1,
        .new_dev_cb = NULL,
    };
    ESP_LOGI(TAG, "Installing the USB CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(&cdc_acm_driver_config));

    while (1) {
        const loader_esp32_usb_cdc_acm_config_t config = {
            .device_vid = ESPRESSIF_VID,
            .device_pid = ESP_SERIAL_JTAG_PID,
            .connection_timeout_ms = 1000,
            .out_buffer_size = 4096,
            .device_disconnected_callback = device_disconnected_callback,
        };

        ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", config.device_vid, config.device_pid);
        if (loader_port_esp32_usb_cdc_acm_init(&config) != ESP_LOADER_SUCCESS) {
            continue;
        }
        vTaskSuspend(NULL);
    }
}

static device_state_t check_device_state(void)
{
    device_state_t state = {0};
    if (eTaskGetState(usbConnectTaskHandle) == eSuspended) {
        state.device_connected = true;
    }

    struct stat st;
    if (stat(MOUNT_POINT, &st) == 0) {
        state.card_mounted = true;
    }
    return state;
}

static void ui_task(void *pvParameter)
{
    encoder_config_t enc_config = {
        .encoder_a_pin = 40,
        .encoder_b_pin = 41,
        .encoder_btn_pin = 42,
    };
    encoder_init(&enc_config);

    while (1) {
        device_state_t state = check_device_state();
        if (state.device_connected && state.card_mounted) {
            screen_set(SELECTOR, 0);
            encoder_t enc = encoder_get_value();
            switch (enc) {
            case ENCODER_MOVE_LEFT:
                selector_roller_change(UP);
                break;
            case ENCODER_MOVE_RIGHT:
                selector_roller_change(DOWN);
                break;
            case ENCODER_CLICKED:
                char buf[32];
                screen_set(FLASHER, 0);
                selector_screen_get_selected(buf, sizeof(buf));
                if (flash_process(buf) != ESP_LOADER_SUCCESS) {
                    screen_set(FLASH_ERROR, "Failed to flash");
                } else {
                    screen_set(FLASH_SUCCESS, "Done!");
                }
                while (encoder_get_value() != ENCODER_CLICKED) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
                break;
            default:
                break;
            }
        } else if (!state.device_connected && !state.card_mounted) {
            screen_set(NOT_READY, "Insert SD card\nand connect a device.");
        } else if (!state.card_mounted) {
            screen_set(NOT_READY, "Insert SD card");
        } else if (!state.device_connected) {
            screen_set(NOT_READY, "Check device connection\nor connect a device.");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void card_mount_task(void *arg)
{
    enum {
        SD_CARD_INSERTED,
        SD_CARD_REMOVED
    } static card_state = SD_CARD_REMOVED;
    while (1) {
        if (card_reader_is_card_inserted() && card_state == SD_CARD_REMOVED) {
            card_state = SD_CARD_INSERTED;
            ESP_ERROR_CHECK(card_reader_mount(MOUNT_POINT));
            ESP_LOGI(TAG, "SD card inserted");
            char *entries = card_reader_get_entries(MOUNT_POINT);
            selector_screen_set_options(entries);
            card_reader_free_entries(entries);
        } else if (!card_reader_is_card_inserted() && card_state == SD_CARD_INSERTED) {
            card_state = SD_CARD_REMOVED;
            ESP_ERROR_CHECK(card_reader_unmount(MOUNT_POINT));
            ESP_LOGI(TAG, "SD card removed");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


// Temporary workaround that resets USB peripheral after an hour, because it stops working after a while
// This issue is also present in esp-serial-flasher
void reset_mcu() {
    esp_restart();
}

void hourly_reset_callback(TimerHandle_t xTimer) {
    reset_mcu();
}

void app_main(void)
{
    display_config_t display_config = {
        .host = SPI3_HOST,
        .spi_frequency = 40000000,
        .pin_num_mosi = 5,
        .pin_num_clk = 6,
        .pin_num_cs = 7,
        .pin_num_dc = 4,
        .pin_num_rst = 8,
        .pin_num_bk_light = 9,
    };

    lvgl_config_t lvgl_config = {
        .lvgl_task_core = 0,
        .lvgl_task_priority = 4,
    };
    display_init(&display_config, &lvgl_config);

    card_reader_config_t card_reader_config = {
        .host = SPI2_HOST,
        .pin_num_miso = 13,
        .pin_num_mosi = 15,
        .pin_num_clk = 1,
        .pin_num_cd = 2,
    };
    card_reader_init(&card_reader_config);

    xTaskCreate(card_mount_task, "card_mount", 4096, NULL, 3, NULL);
    xTaskCreatePinnedToCore(usb_connect_task, "usb_connect", 4096, NULL, 1, &usbConnectTaskHandle, 1);
    xTaskCreatePinnedToCore(ui_task, "ui_task", 4096, NULL, 5, NULL, 0);

    // Create timer with 1,5 hour period that resets mcu - temporary workaround
    TimerHandle_t usb_timer = xTimerCreate("USBReset", pdMS_TO_TICKS(5400000), pdTRUE, NULL, hourly_reset_callback);
    xTimerStart(usb_timer, 0);
}