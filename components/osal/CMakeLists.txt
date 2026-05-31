idf_component_register(
    SRCS "src/osal_freertos.c"
    INCLUDE_DIRS "include"
    REQUIRES
        core
    PRIV_REQUIRES
        board
        esp_timer
        freertos
)
