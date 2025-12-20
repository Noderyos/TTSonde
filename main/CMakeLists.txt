idf_component_register(
        SRCS "main.c" "utils.c" "storage.c" "screen.c" "wifi.c" "gps.c" "http.c" "vbat.c"
             "ui/ui.c"
             "sonde/m20.c"
        INCLUDE_DIRS "include"
        REQUIRES sx127x esp_timer nvs_flash esp_wifi esp_adc
)
littlefs_create_partition_image(storage ../flash_data FLASH_IN_PROJECT)