#pragma once
#include "lvgl.h"

/* 卡片轮播菜单（Layer 2），3 张卡片循环导航 */
void card_menu_show(void);

/* 彻底释放菜单所有 LVGL 资源 */
void card_menu_destroy(void);
