#include "lvgl_cmd.hpp"

#define LVGL_CMD_QUEUE_LEN 16
#define LVGL_DEFER_QUEUE_LEN 8

struct LvglCmd {
    lvgl_cmd_fn_t fn;
    void*         arg;
};

static QueueHandle_t s_cmd_queue   = nullptr;
static QueueHandle_t s_defer_queue = nullptr;

void lvgl_cmd_init(void)
{
    s_cmd_queue = xQueueCreate(LVGL_CMD_QUEUE_LEN, sizeof(LvglCmd));
    configASSERT(s_cmd_queue != nullptr);

    s_defer_queue = xQueueCreate(LVGL_DEFER_QUEUE_LEN, sizeof(LvglCmd));
    configASSERT(s_defer_queue != nullptr);
}

bool lvgl_cmd_post(lvgl_cmd_fn_t fn, void* arg)
{
    if (!s_cmd_queue || !fn) return false;
    LvglCmd cmd = {fn, arg};
    return xQueueSend(s_cmd_queue, &cmd, 0) == pdTRUE;
}

bool lvgl_cmd_process(void)
{
    if (!s_cmd_queue) return false;
    LvglCmd cmd;
    if (xQueueReceive(s_cmd_queue, &cmd, 0) != pdTRUE) return false;
    if (cmd.fn) cmd.fn(cmd.arg);
    return true;
}

bool lvgl_defer_post(lvgl_cmd_fn_t fn, void* arg)
{
    if (!s_defer_queue || !fn) return false;
    LvglCmd cmd = {fn, arg};
    return xQueueSend(s_defer_queue, &cmd, 0) == pdTRUE;
}

bool lvgl_defer_process(void)
{
    if (!s_defer_queue) return false;
    LvglCmd cmd;
    if (xQueueReceive(s_defer_queue, &cmd, 0) != pdTRUE) return false;
    if (cmd.fn) cmd.fn(cmd.arg);
    return true;
}