#pragma once

#include "task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class TaskManager
{
public:
    using TaskEntry = void (*)(void* param);

    static TaskHandle_t create(const board_task_config_t& config, TaskEntry entry, void* param);
};
