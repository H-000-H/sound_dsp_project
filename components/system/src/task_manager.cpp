#include "task_manager.hpp"

#include "esp_heap_caps.h"
#include "system_log.hpp"

static constexpr const char* kTag = "TaskManager";

TaskHandle_t TaskManager::create(const board_task_config_t& config, TaskEntry entry, void* param)
{
    if (entry == nullptr)
    {
        SYS_LOGE(kTag, "task entry is null: %s", config.name);
        return nullptr;
    }

    StackType_t* stack = static_cast<StackType_t*>(
        heap_caps_malloc(config.stack_size, MALLOC_CAP_SPIRAM));
    StaticTask_t* tcb = static_cast<StaticTask_t*>(
        heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    if (stack != nullptr && tcb != nullptr)
    {
        return xTaskCreateStaticPinnedToCore(
            entry,
            config.name,
            config.stack_size,
            param,
            config.priority,
            stack,
            tcb,
            config.core_id);
    }

    if (stack != nullptr)
    {
        heap_caps_free(stack);
    }
    if (tcb != nullptr)
    {
        heap_caps_free(tcb);
    }

    TaskHandle_t handle = nullptr;
    const BaseType_t ret = xTaskCreatePinnedToCore(
        entry,
        config.name,
        config.stack_size,
        param,
        config.priority,
        &handle,
        config.core_id);

    if (ret != pdPASS)
    {
        SYS_LOGE(kTag, "failed to create task: %s", config.name);
        return nullptr;
    }

    return handle;
}
