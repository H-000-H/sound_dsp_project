idf_component_register(
    SRCS
        "src/audio_engine.cpp"
        "src/render_engine.cpp"
        "src/led_engine.cpp"
        "src/input_engine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        drivers
        algorithm
        media
    PRIV_REQUIRES
        esp_timer
)
