#ifndef TASK_CONFIG_H
#define TASK_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── RMS 单调速率调度优先级 (数值越高优先级越高)
 * 工业/医疗级双核隔离策略:
 *   PRO_CPU (Core 0): OS内核 / LWIP网络栈 / 驱动回调 / MQTT
 *   APP_CPU (Core 1): LVGL UI渲染 / 音频解码 / I2S DMA喂数据
 * ── */
#define TASK_PRIO_IDLE          1   /* 空闲 / 后台日志 */
#define TASK_PRIO_CLOUD         5   /* MQTT / 云端同步 */
#define TASK_PRIO_UI_RENDER     8   /* LVGL UI 渲染 */
#define TASK_PRIO_AUDIO_DECODE  10  /* MP3 后台解码 */
#define TASK_PRIO_NETWORK       15  /* LWIP + WiFi 事件处理 */
#define TASK_PRIO_AUDIO_FEED    20  /* I2S DMA 喂数据 (最高频 ~22kHz) */

#define CORE_PRO_CPU 0
#define CORE_APP_CPU 1

typedef struct
{
    const char* name;
    uint32_t stack_size;
    uint32_t priority;
    int8_t core_id;
} board_task_config_t;

/* PRO_CPU (Core 0): 网络 / 云端 / 驱动 */
extern const board_task_config_t board_task_cloud;
extern const board_task_config_t board_task_network;

/* APP_CPU (Core 1): UI / 音频 */
extern const board_task_config_t board_task_ui;
extern const board_task_config_t board_task_audio_decode;
extern const board_task_config_t board_task_audio_feed;

#ifdef __cplusplus
}
#endif

#endif /* TASK_CONFIG_H */
