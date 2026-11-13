idf_component_register(SRCS "net_name.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common
                       PRIV_REQUIRES mdns log)
