#pragma once
#include <cstdint>

enum AppTheme : uint8_t { THEME_DARK = 0, THEME_LIGHT = 1 };

void theme_set(AppTheme t);
AppTheme theme_get();

/* 颜色 getter — 返回当前主题对应的 hex 值 */
uint32_t th_bg();          /* 屏幕背景 */
uint32_t th_card();         /* 卡片/控件背景 */
uint32_t th_card_hl();      /* 卡片高亮/选中背景 */
uint32_t th_text();         /* 主文字 */
uint32_t th_text_sec();     /* 次要文字 */
uint32_t th_text_dim();     /* 更淡文字 */
uint32_t th_border();       /* 边框 */
uint32_t th_border_dim();   /* 淡边框 */
uint32_t th_accent();       /* 强调色 */
uint32_t th_term_bg();      /* 终端/输出区域背景（深色保持类终端外观） */
