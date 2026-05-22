idf_component_register(
    SRCS
        "src/service_registry.cpp"
        "src/system_runtime.cpp"
        "src/task_manager.cpp"
        "src/board_driver_list.c"
    INCLUDE_DIRS
        "include"
    REQUIRES board drivers middleware service freertos esp_timer
)
