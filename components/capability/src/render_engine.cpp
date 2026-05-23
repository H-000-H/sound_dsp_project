#include "capability/render_engine.hpp"

#include "device.h"
#include "st7789_driver.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "render_engine";

typedef struct {
    device_t* lcd_dev;
    int width;
    int height;
} render_engine_impl_t;

static int eng_init(render_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    render_engine_impl_t* impl = (render_engine_impl_t*)eng->_impl;

    impl->lcd_dev = device_find("lcd0");
    if (!impl->lcd_dev)
    {
        ESP_LOGE(kTag, "lcd0 not found");
        return -1;
    }
    if (st7789_init(impl->lcd_dev) != 0)
    {
        ESP_LOGE(kTag, "st7789 init failed");
        return -1;
    }

    st7789_info_t info;
    st7789_get_info(impl->lcd_dev, &info);
    impl->width = info.width;
    impl->height = info.height;

    st7789_fill_screen(impl->lcd_dev, 0x0000);
    ESP_LOGI(kTag, "render engine ready: %dx%d", impl->width, impl->height);
    return 0;
}

static int eng_flush(render_engine_t* eng, int x, int y, int w, int h, const uint16_t* pixels)
{
    if (!eng || !eng->_impl || !pixels) return -1;
    render_engine_impl_t* impl = (render_engine_impl_t*)eng->_impl;
    return st7789_write_ram(impl->lcd_dev, x, y, w, h, pixels);
}

static int eng_fill_screen(render_engine_t* eng, uint16_t color)
{
    if (!eng || !eng->_impl) return -1;
    render_engine_impl_t* impl = (render_engine_impl_t*)eng->_impl;
    return st7789_fill_screen(impl->lcd_dev, color);
}

static int eng_set_backlight(render_engine_t* eng, uint8_t brightness)
{
    if (!eng || !eng->_impl) return -1;
    render_engine_impl_t* impl = (render_engine_impl_t*)eng->_impl;
    return st7789_set_backlight(impl->lcd_dev, brightness);
}

static int eng_get_display_size(render_engine_t* eng, int* width, int* height)
{
    if (!eng || !eng->_impl) return -1;
    render_engine_impl_t* impl = (render_engine_impl_t*)eng->_impl;
    if (width) *width = impl->width;
    if (height) *height = impl->height;
    return 0;
}

static void eng_deinit(render_engine_t* eng)
{
    if (!eng || !eng->_impl) return;
    render_engine_impl_t* impl = (render_engine_impl_t*)eng->_impl;
    st7789_fill_screen(impl->lcd_dev, 0x0000);
    free(impl);
    eng->_impl = NULL;
}

void render_engine_init_struct(render_engine_t* eng)
{
    if (!eng) return;
    eng->init = eng_init;
    eng->flush = eng_flush;
    eng->fill_screen = eng_fill_screen;
    eng->set_backlight = eng_set_backlight;
    eng->get_display_size = eng_get_display_size;
    eng->deinit = eng_deinit;
    eng->_impl = calloc(1, sizeof(render_engine_impl_t));
}
