idf_component_register(
    SRCS
        "network/src/tcp_client.cpp"
        "protocol/src/mqtt_client.cpp"
    INCLUDE_DIRS
        "factory"
        "network/inc"
        "protocol/inc"
    REQUIRES device_hal freertos json mqtt config
    PRIV_REQUIRES bsp
)
