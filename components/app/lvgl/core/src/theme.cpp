#include "theme.hpp"

static AppTheme s_theme = THEME_DARK;

void theme_set(AppTheme t) { s_theme = t; }
AppTheme theme_get() { return s_theme; }

/* 颜色表
   Role        | Dark            | Light (柔和)
   ------------+-----------------+----------------*/
uint32_t th_bg()
{
    return s_theme == THEME_LIGHT ? 0xE4E5E9 : 0x0E0F14;
}
uint32_t th_card()
{
    return s_theme == THEME_LIGHT ? 0xF0F1F4 : 0x1C1C22;
}
uint32_t th_card_hl()
{
    return s_theme == THEME_LIGHT ? 0xE2E3E8 : 0x2C2F3E;
}
uint32_t th_text()
{
    return s_theme == THEME_LIGHT ? 0x2C2C30 : 0xF5F5F7;
}
uint32_t th_text_sec()
{
    return 0x8E8E93;  /* 深浅主题相同 */
}
uint32_t th_text_dim()
{
    return s_theme == THEME_LIGHT ? 0x8E8E93 : 0x5C5F73;
}
uint32_t th_border()
{
    return s_theme == THEME_LIGHT ? 0xD1D1D6 : 0x2C2F3E;
}
uint32_t th_border_dim()
{
    return s_theme == THEME_LIGHT ? 0xE5E5EA : 0x3A3D4E;
}
uint32_t th_accent()
{
    return s_theme == THEME_LIGHT ? 0x007AFF : 0xFF9F0A;
}
uint32_t th_term_bg()
{
    return s_theme == THEME_LIGHT ? 0xF0F1F4 : 0x08090C;
}
