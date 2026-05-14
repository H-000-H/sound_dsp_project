#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string>
#include <esp_log.h>
#include "factory.hpp"
#include "pwm_controller.hpp"
#include "thingscloud_app.hpp"
#include "lvgl_main.hpp"
#define CORE_ONE 0
#define CORE_TWO 1
void app_freertos_init();
