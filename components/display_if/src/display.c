#include "display.h"
#include "VFS.h"

/* 从 subsys_priv 魔术头提取 ops 表 (MISRA C 11.3 合规: 显式指针, 无隐式偏移假设) */
static const struct display_ops* get_display_ops(device_t* dev)
{
    if (!dev) return NULL;
    display_if_priv_t* hdr = (display_if_priv_t*)device_get_subsys_priv(dev);
    if (!hdr || hdr->magic != DISPLAY_IF_MAGIC || !hdr->ops) return NULL;
    return hdr->ops;
}

int display_fill_rect(device_t* dev, int x, int y, int w, int h, uint16_t color, uint32_t timeout_ms)
{
    const struct display_ops* ops = get_display_ops(dev);
    if (!ops || !ops->fill_rect) return VFS_ERR_INVAL;
    return ops->fill_rect(dev, x, y, w, h, color, timeout_ms);
}

int display_fill_screen(device_t* dev, uint16_t color, uint32_t timeout_ms)
{
    const struct display_ops* ops = get_display_ops(dev);
    if (!ops || !ops->fill_screen) return VFS_ERR_INVAL;
    return ops->fill_screen(dev, color, timeout_ms);
}

int display_draw_bitmap(device_t* dev, int x, int y, int w, int h, const uint8_t* data, uint32_t timeout_ms)
{
    const struct display_ops* ops = get_display_ops(dev);
    if (!ops || !ops->draw_bitmap) return VFS_ERR_INVAL;
    return ops->draw_bitmap(dev, x, y, w, h, data, timeout_ms);
}

int display_write_ram(device_t* dev, int x, int y, int w, int h, const uint16_t* pixels, uint32_t timeout_ms)
{
    const struct display_ops* ops = get_display_ops(dev);
    if (!ops || !ops->write_ram) return VFS_ERR_INVAL;
    return ops->write_ram(dev, x, y, w, h, pixels, timeout_ms);
}

int display_set_backlight(device_t* dev, uint8_t brightness)
{
    const struct display_ops* ops = get_display_ops(dev);
    if (!ops || !ops->set_backlight) return VFS_ERR_INVAL;
    return ops->set_backlight(dev, brightness);
}

int display_get_info(device_t* dev, display_info_t* info)
{
    const struct display_ops* ops = get_display_ops(dev);
    if (!ops || !ops->get_info) return VFS_ERR_INVAL;
    return ops->get_info(dev, info);
}
