idf_component_register(SRCS "led_output.c"
                       INCLUDE_DIRS "include"
                       REQUIRES led_signal esp_common
                       PRIV_REQUIRES led_strip log)
