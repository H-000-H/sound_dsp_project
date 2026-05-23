#ifndef CAPABILITY_RENDER_ENGINE_H
#define CAPABILITY_RENDER_ENGINE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 渲染能力 — 组合显示驱动 + 帧缓冲管理
 *
 * 位于 service 和 driver 之间:
 *   UiService → RenderEngine → st7789_driver + LVGL
 */
typedef struct render_engine render_engine_t;

struct render_engine {
    int (*init)(render_engine_t* eng);
    int (*flush)(render_engine_t* eng, int x, int y, int w, int h, const uint16_t* pixels);
    int (*fill_screen)(render_engine_t* eng, uint16_t color);
    int (*set_backlight)(render_engine_t* eng, uint8_t brightness);
    int (*get_display_size)(render_engine_t* eng, int* width, int* height);
    void (*deinit)(render_engine_t* eng);
    void* _impl;
};

void render_engine_init_struct(render_engine_t* eng);

#ifdef __cplusplus
}
#endif

#endif /* CAPABILITY_RENDER_ENGINE_H */
