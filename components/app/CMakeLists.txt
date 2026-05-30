file(GLOB_RECURSE app_srcs
    "src/*.cpp"
    "lvgl/core/src/*.cpp"
    "lvgl/UI/screen/src/*.cpp"
    "lvgl/UI/nav/src/*.cpp"
    "lvgl/UI/app/src/*.cpp"
    "lvgl/UI/app/music/*.cpp"
    "lvgl/UI/app/serial/*.cpp"
    "lvgl/res/picture/*.c"
    "lvgl/res/font/*.c"
    "lvgl/res/lottie/*.c"
)

idf_component_register(
    SRCS ${app_srcs}
    INCLUDE_DIRS
        "inc"
        "lvgl"
        "lvgl/core/inc"
        "lvgl/ui"
        "lvgl/UI/app"
        "lvgl/UI/app/inc"
        "lvgl/UI/nav/inc"
        "lvgl/UI/screen/inc"
        "lvgl/res/picture"
        "lvgl/res/lottie"
    REQUIRES core freertos service system board drivers hal_if json lvgl esp_netif esp_wifi esp_event nvs_flash algorithm
    EMBED_TXTFILES "lvgl/res/lottie/setting.json"
)
