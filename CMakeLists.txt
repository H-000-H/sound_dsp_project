cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -DLV_USE_LOTTIE=0)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -DLV_USE_LOTTIE=0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sound_dsp_project)

# Workaround for GCC 14.2 internal compiler error (ICE) on esp_lcd_panel_rgb.c
set_source_files_properties(
    ${IDF_PATH}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c
    PROPERTIES COMPILE_FLAGS "-O0"
)

# -u 必须在 .a 之前, INTERFACE 会追加到行尾, 用 PRIVATE BEFORE 放到行首
target_link_options(sound_dsp_project.elf PRIVATE
    "-u board_driver_probe_light_sensor"
    "-u board_driver_probe_pwm_bl"
    "-u board_driver_probe_gpio"
    "-u board_driver_probe_i2c"
    "-u board_driver_probe_spi"
    "-u board_driver_probe_i2s"
    "-u board_driver_probe_uart"
    "-u board_driver_probe_rmt_led"
    "-u board_driver_probe_st7789"
    "-u board_driver_probe_max98357a"
    "-u board_driver_probe_gpio_key"
    "-u board_driver_probe_ws2812"
)
