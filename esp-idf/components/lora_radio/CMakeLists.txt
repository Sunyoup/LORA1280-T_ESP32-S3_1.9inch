idf_component_register(SRCS "lora_radio.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES jgromes__radiolib esp_driver_gpio esp_timer)
