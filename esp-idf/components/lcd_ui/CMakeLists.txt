idf_component_register(SRCS "lcd.c" "ui.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd esp_driver_spi esp_timer board)
