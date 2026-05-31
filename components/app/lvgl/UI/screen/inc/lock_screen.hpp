#pragma once
#include "lvgl.h"

/* 锁屏界面（Layer 1） */
void lock_screen_show(void);

/* 彻底释放锁屏所有 LVGL 资源 */
void lock_screen_destroy(void);
