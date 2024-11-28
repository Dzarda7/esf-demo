#pragma once

#define MOUNT_POINT "/sdcard"

#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_host_device_t host;
    uint32_t pin_num_miso;
    uint32_t pin_num_mosi;
    uint32_t pin_num_clk;
    uint32_t pin_num_cs;
    uint32_t pin_num_cd;
} card_reader_config_t;

void card_reader_init(card_reader_config_t *config);
bool card_reader_is_card_inserted(void);
esp_err_t card_reader_mount(const char *mount_point);
esp_err_t card_reader_unmount(const char *mount_point);
char *card_reader_get_entries(const char *mount_point);
void card_reader_free_entries(char *dir_names);

#ifdef __cplusplus
}
#endif