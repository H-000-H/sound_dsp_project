#pragma once
#include "lvgl.h"
#include <stdint.h>

/* 状态栏：WiFi/蓝牙/电量图标（在 lv_layer_top() 上）*/
void status_bar_init(void);

/* 由外部任务调用，通知连接状态变化 */
void ui_set_wifi_state(bool connected);
void ui_set_bt_state(bool connected);
void ui_set_battery_level(int8_t percent);

/* 全局音量 */
int ui_get_volume(void);
