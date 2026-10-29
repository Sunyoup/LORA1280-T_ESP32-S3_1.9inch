idf_component_register(SRCS "main.cpp" "role_tx.cpp" "role_rx.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES lora_radio lcd_ui board nvs_flash esp_timer)
