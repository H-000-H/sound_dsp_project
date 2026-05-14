#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifndef    USE_SINGE_BUFFER
#define    USE_SINGE_BUFFER     1
#endif
#ifndef    USE_DOUBLE_BUFFER
#define    USE_DOUBLE_BUFFER    0
#endif
#ifndef    USE_LVGL_PRAM
#define    USE_LVGL_PRAM        0
#endif
#ifndef    USE_LVGL_DMA
#define    USE_LVGL_DMA         1
#endif

void lvgl_mutex_lock();
void lvgl_mutex_unlock();
void lvgl_mutex_init();
