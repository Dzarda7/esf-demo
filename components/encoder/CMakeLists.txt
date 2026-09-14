idf_component_register(SRCS "encoder.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES "driver" "esp_driver_pcnt" "freertos" "button")
