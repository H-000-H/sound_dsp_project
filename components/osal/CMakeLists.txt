idf_component_register(
    SRCS "src/osal_freertos.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        esp_timer
        freertos
)
