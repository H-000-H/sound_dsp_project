idf_component_register(
    INCLUDE_DIRS "include" "."
    SRCS
        "src/config_store.cpp"
    PRIV_REQUIRES freertos
    EMBED_TXTFILES "../../assets/config/system_config.json"
)
