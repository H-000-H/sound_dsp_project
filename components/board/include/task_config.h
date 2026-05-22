#ifndef TASK_CONFIG_H
#define TASK_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char* name;
    uint32_t stack_size;
    uint32_t priority;
    int core_id;
} board_task_config_t;

extern const board_task_config_t board_task_ui;
extern const board_task_config_t board_task_cloud;

#ifdef __cplusplus
}
#endif

#endif /* TASK_CONFIG_H */
