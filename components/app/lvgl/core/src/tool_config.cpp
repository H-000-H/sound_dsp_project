#include "tool_config.hpp"
static SemaphoreHandle_t lvgl_mutex = nullptr;

void lvgl_mutex_lock()
{
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
}

void lvgl_mutex_unlock()
{
    xSemaphoreGive(lvgl_mutex);
}

void lvgl_mutex_init()
{
    lvgl_mutex = xSemaphoreCreateMutex();
    configASSERT(lvgl_mutex != nullptr);
}
