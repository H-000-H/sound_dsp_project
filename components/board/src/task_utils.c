#include "task_utils.h"
#include "osal.h"

void* board_task_create(const char* name, uint32_t stack_size,
                        uint32_t priority, board_task_entry_t entry,
                        void* param, int core_id)
{
    int ret = osal_task_create(name, stack_size, priority,
                               (osal_task_entry_t)entry, param, core_id);
    return (ret == 0) ? (void*)1 : NULL;
}
