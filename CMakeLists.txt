cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -DLV_USE_LOTTIE=0)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -DLV_USE_LOTTIE=0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/components/soc_port_esp32)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sound_dsp_project)

# Workaround for GCC 14.2 internal compiler error (ICE) on esp_lcd_panel_rgb.c
set_source_files_properties(
    ${IDF_PATH}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c
    PROPERTIES COMPILE_FLAGS "-O0"
)

# -u 必须在 .a 之前, INTERFACE 会追加到行尾, 用 PRIVATE BEFORE 放到行首
target_link_options(sound_dsp_project.elf PRIVATE
    "-Wl,--undefined=board_driver_probe_light_sensor"
    "-Wl,--undefined=board_driver_probe_pwm_bl"
    "-Wl,--undefined=board_driver_probe_gpio"
    "-Wl,--undefined=board_driver_probe_i2c"
    "-Wl,--undefined=board_driver_probe_spi"
    "-Wl,--undefined=board_driver_probe_i2s"
    "-Wl,--undefined=board_driver_probe_uart"
    "-Wl,--undefined=board_driver_probe_rmt_led"
    "-Wl,--undefined=board_driver_probe_adc"
    "-Wl,--undefined=board_driver_probe_st7789"
    "-Wl,--undefined=board_driver_probe_max98357a"
    "-Wl,--undefined=board_driver_probe_gpio_key"
    "-Wl,--undefined=board_driver_probe_ws2812"
)
