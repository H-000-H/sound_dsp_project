#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include <stdint.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 公开信息 ── */
typedef struct {
    int width;
    int height;
} st7789_info_t;

/* ── API ── */
int st7789_init(device_t* dev);
int st7789_get_info(device_t* dev, st7789_info_t* info);

/* 基本绘图 */
int st7789_fill_rect(device_t* dev, int x, int y, int w, int h, uint16_t color);
int st7789_draw_bitmap(device_t* dev, int x, int y, int w, int h, const uint8_t* data);
int st7789_fill_screen(device_t* dev, uint16_t color);
int st7789_set_backlight(device_t* dev, uint8_t brightness);

/* 底层寄存器写 (给 LVGL disp_flush 用) */
int st7789_write_ram(device_t* dev, int x, int y, int w, int h, const uint16_t* pixels);

#ifdef __cplusplus
}
#endif

#endif /* ST7789_DRIVER_H */
