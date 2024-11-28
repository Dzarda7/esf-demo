#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "card_reader.h"

static const char *TAG = "card_reader";
static SemaphoreHandle_t card_reader_mutex;
static sdmmc_card_t *card;

#define SD_HOST       SPI2_HOST
#define PIN_NUM_MISO  13
#define PIN_NUM_MOSI  15
#define PIN_NUM_CLK   1
#define PIN_NUM_CS    2
#define PIN_NUM_CD    3
static card_reader_config_t s_config;

esp_err_t card_reader_mount(const char *mount_point)
{
    if (xSemaphoreTake(card_reader_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex.");
        return ESP_FAIL;
    }
    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = s_config.host;

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = s_config.pin_num_cs;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 1,
        .allocation_unit_size = 16 * 1024
    };

    esp_err_t ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
        xSemaphoreGive(card_reader_mutex);
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    xSemaphoreGive(card_reader_mutex);
    return ESP_OK;
}

esp_err_t card_reader_unmount(const char *mount_point)
{
    if (xSemaphoreTake(card_reader_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex.");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
        xSemaphoreGive(card_reader_mutex);
        return ret;
    }

    ESP_LOGI(TAG, "SD card unmounted successfully");
    xSemaphoreGive(card_reader_mutex);
    return ESP_OK;
}

bool card_reader_is_card_inserted(void)
{
    if (xSemaphoreTake(card_reader_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex.");
        return false;
    }
    int card_inserted = gpio_get_level(s_config.pin_num_cd) == 0;
    xSemaphoreGive(card_reader_mutex);
    return card_inserted;
}

void card_reader_init(card_reader_config_t *config)
{
    ESP_LOGI(TAG, "Initializing SD card");

    card_reader_mutex = xSemaphoreCreateMutex();
    if (card_reader_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex.");
        return;
    }

    memcpy(&s_config, config, sizeof(card_reader_config_t));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = s_config.pin_num_mosi,
        .miso_io_num = s_config.pin_num_miso,
        .sclk_io_num = s_config.pin_num_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(s_config.host, &bus_cfg, SDSPI_DEFAULT_DMA));

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << s_config.pin_num_cd,
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
                             .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

char *card_reader_get_entries(const char *mount_point)
{
    if (xSemaphoreTake(card_reader_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex.");
        return NULL;
    }

    DIR *dir;
    if ((dir = opendir(mount_point)) == NULL) {
        perror("opendir");
        xSemaphoreGive(card_reader_mutex);
        return NULL;
    }

    struct dirent *entry;
    char *dir_names = NULL;
    size_t total_length = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            // Skip hidden files
            continue;
        }

        if (entry->d_type != DT_DIR) {
            // Entry is not a directory
            continue;
        }

        size_t name_length = strlen(entry->d_name);
        dir_names = realloc(dir_names, total_length + name_length + 1); // +1 for '\n'
        if (dir_names == NULL) {
            perror("realloc");
            closedir(dir);
            xSemaphoreGive(card_reader_mutex);
            return NULL;
        }

        strcpy(dir_names + total_length, entry->d_name);
        total_length += name_length;
        dir_names[total_length] = '\n';
        total_length++;
    }

    // Null-terminate the final string (remove the last '\n')
    if (dir_names != NULL) {
        dir_names[total_length - 1] = '\0';
    }

    closedir(dir);

    xSemaphoreGive(card_reader_mutex);
    return dir_names;
}

void card_reader_free_entries(char *dir_names)
{
    if (xSemaphoreTake(card_reader_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex.");
        return;
    }

    free(dir_names);

    xSemaphoreGive(card_reader_mutex);
}
