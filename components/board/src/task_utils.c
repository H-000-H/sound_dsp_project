#include "task_utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void* board_task_create(const char* name, uint32_t stack_size,
                        uint32_t priority, board_task_entry_t entry,
                        void* param, int core_id)
{
    TaskHandle_t handle = NULL;
    BaseType_t ret = xTaskCreatePinnedToCore(entry, name, stack_size,
                                             param, priority, &handle, core_id);
    if (ret != pdPASS) return NULL;
    return (void*)handle;
}
