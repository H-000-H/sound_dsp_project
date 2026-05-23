#pragma once

#include "task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class TaskManager
{
public:
    using TaskEntry = void (*)(void* param);

    static TaskHandle_t create(const board_task_config_t& config, TaskEntry entry, void* param);

    /** 简便版: 从参数直接创建任务, 无需预先定义 board_task_config_t */
    static TaskHandle_t create_task(const char* name, uint32_t stack_size,
                                    uint32_t priority, TaskEntry entry,
                                    void* param, int core_id);
};
