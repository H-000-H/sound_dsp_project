#include "task_manager.hpp"

#include "system_log.hpp"

static constexpr const char* kTag = "TaskManager";

TaskHandle_t TaskManager::create(const board_task_config_t& config, TaskEntry entry, void* param)
{
    if (entry == nullptr)
    {
        SYS_LOGE(kTag, "task entry is null: %s", config.name);
        return nullptr;
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

TaskHandle_t TaskManager::create_task(const char* name, uint32_t stack_size,
                                       uint32_t priority, TaskEntry entry,
                                       void* param, int core_id)
{
    board_task_config_t cfg = {};
    cfg.name = name ? name : "unknown";
    cfg.stack_size = stack_size;
    cfg.priority = priority;
    cfg.core_id = core_id;
    return create(cfg, entry, param);
}
