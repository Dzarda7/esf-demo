idf_component_register(SRCS "display.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES "esp_lcd" "driver" "freertos" "esp_lvgl_port" "esp_lcd_gc9a01")
