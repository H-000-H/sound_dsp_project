#include "task_config.h"

/* ── PRO_CPU (Core 0): 网络栈 / 云端消息 ── */
const board_task_config_t board_task_cloud = {
    .name = "cloud_task",
    .stack_size = 6 * 1024,
    .priority = TASK_PRIO_CLOUD,
    .core_id = CORE_PRO_CPU,
};

const board_task_config_t board_task_network = {
    .name = "net_task",
    .stack_size = 4 * 1024,
    .priority = TASK_PRIO_NETWORK,
    .core_id = CORE_PRO_CPU,
};

/* ── APP_CPU (Core 1): UI + 音频 ── */
const board_task_config_t board_task_ui = {
    .name = "ui_task",
    .stack_size = 24 * 1024,
    .priority = TASK_PRIO_UI_RENDER,
    .core_id = CORE_APP_CPU,
};

const board_task_config_t board_task_audio_decode = {
    .name = "audio_dec",
    .stack_size = 16 * 1024,
    .priority = TASK_PRIO_AUDIO_DECODE,
    .core_id = CORE_APP_CPU,
};

const board_task_config_t board_task_audio_feed = {
    .name = "audio_feed",
    .stack_size = 4 * 1024,
    .priority = TASK_PRIO_AUDIO_FEED,
    .core_id = CORE_APP_CPU,
};