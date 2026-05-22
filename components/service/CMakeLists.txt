idf_component_register(
    SRCS
        "network/src/tcp_client.cpp"
        "protocol/src/mqtt_client.cpp"
        "audio/src/mp3.cpp"
        "audio/src/audio_service.cpp"
        "ui/src/ui_service.cpp"
        "cloud/src/cloud_service.cpp"
    INCLUDE_DIRS
        "factory"
        "network/inc"
        "protocol/inc"
        "audio/inc"
        "ui/inc"
        "cloud/inc"
    REQUIRES board algorithm drivers middleware hw_hal freertos json mqtt config chmorgan__esp-libhelix-mp3 lvgl__lvgl esp_wifi esp_netif esp_event nvs_flash
)
