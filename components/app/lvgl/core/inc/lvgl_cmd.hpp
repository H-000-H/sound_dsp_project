#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*lvgl_cmd_fn_t)(void*);

void lvgl_cmd_init(void);

bool lvgl_cmd_post(lvgl_cmd_fn_t fn, void* arg);

bool lvgl_cmd_process(void);

bool lvgl_defer_post(lvgl_cmd_fn_t fn, void* arg);

bool lvgl_defer_process(void);

#ifdef __cplusplus
}
#endif