# Both board files are compiled; each is wrapped in #if RG_BOARD_1_28 (set in fw/CMakeLists.txt).
idf_component_register(SRCS "board_2_1.c" "board_1_28.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_gpio esp_lcd espressif__esp_lcd_touch
                    PRIV_REQUIRES esp_driver_i2c esp_driver_ledc esp_driver_spi esp_driver_sdspi fatfs sdmmc vfs)
