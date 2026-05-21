idf_component_register(
    SRCS
        "network/src/tcp_client.cpp"
        "protocol/src/mqtt_client.cpp"
        "audio/src/mp3.cpp"
    INCLUDE_DIRS
        "factory"
        "network/inc"
        "protocol/inc"
        "audio/inc"
    REQUIRES device_hal algorithm freertos json mqtt config chmorgan__esp-libhelix-mp3
    PRIV_REQUIRES bsp
)
