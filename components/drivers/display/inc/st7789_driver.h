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

/* ── ioctl 命令 ── */
#define ST7789_CMD_GET_INFO      1  /* arg: st7789_info_t* */
#define ST7789_CMD_FILL_RECT     2  /* arg: st7789_fill_rect_arg_t* */
#define ST7789_CMD_FILL_SCREEN   3  /* arg: int* — color */
#define ST7789_CMD_DRAW_BITMAP   4  /* arg: st7789_draw_bitmap_arg_t* */
#define ST7789_CMD_SET_BACKLIGHT 5  /* arg: int* — brightness */
#define ST7789_CMD_WRITE_RAM     6  /* arg: st7789_write_ram_arg_t* */

/* ── ioctl 参数结构 ── */
typedef struct {
    int x, y, w, h;
    uint16_t color;
} st7789_fill_rect_arg_t;

typedef struct {
    int x, y, w, h;
    const uint8_t* data;
} st7789_draw_bitmap_arg_t;

typedef struct {
    int x, y, w, h;
    const uint16_t* pixels;
} st7789_write_ram_arg_t;

#ifdef __cplusplus
}
#endif

#endif /* ST7789_DRIVER_H */
