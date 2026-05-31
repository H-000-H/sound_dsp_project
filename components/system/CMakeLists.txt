idf_component_register(
    SRCS
        "src/lifecycle.cpp"
        "src/system_runtime.cpp"
        "src/system_wdt.cpp"
        "src/task_manager.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES board drivers core service freertos esp_timer esp_netif esp_event
)
