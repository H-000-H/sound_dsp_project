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
