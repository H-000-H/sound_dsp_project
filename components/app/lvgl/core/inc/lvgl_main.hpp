#pragma once
#include <stddef.h>
#include "lvgl.h"
#include "esp_timer.h"
#include "tool_config.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "factory.hpp"
#include "esp_heap_caps.h"

constexpr uint16_t screen_width = 240;
constexpr uint16_t screen_height = 240;
constexpr uint8_t  RGB565_BYTE  = 2;

void lvgl_main();

/** 将耗时操作推迟到 lv_timer_handler() 之后、LVGL 互斥锁之外执行。 */
void lvgl_defer(void (*fn)(void*), void* arg);
