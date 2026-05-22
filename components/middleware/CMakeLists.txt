idf_component_register(
    SRCS
        "src/config_store.cpp"
        "src/event_bus.cpp"
        "src/key_input.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES board drivers esp_timer
    EMBED_TXTFILES "../../assets/config/system_config.json"
)
