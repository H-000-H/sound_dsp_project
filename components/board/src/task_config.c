#include "task_config.h"

const board_task_config_t board_task_ui = {
    .name = "ui_task",
    .stack_size = 24 * 1024,
    .priority = 5,
    .core_id = 0,
};

const board_task_config_t board_task_cloud = {
    .name = "cloud_task",
    .stack_size = 4 * 1024,
    .priority = 5,
    .core_id = 1,
};
