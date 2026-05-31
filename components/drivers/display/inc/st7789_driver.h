#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include "display.h"

#define ST7789_TIMEOUT_CMD_MS       1000
#define ST7789_TIMEOUT_IOCTL_MS     100
#define ST7789_TIMEOUT_POWEROFF_MS  500
#define ST7789_TIMEOUT_SLEEP_MS     5
#define ST7789_TIMEOUT_WAKE_MS      120

/*
 * st7789 驱动公共接口已迁移至 display.h 强类型子系统 API.
 *
 * 替换对照:
 *   ST7789_CMD_FILL_RECT    → display_fill_rect()
 *   ST7789_CMD_FILL_SCREEN  → display_fill_screen()
 *   ST7789_CMD_DRAW_BITMAP  → display_draw_bitmap()
 *   ST7789_CMD_WRITE_RAM    → display_write_ram()
 *   ST7789_CMD_SET_BACKLIGHT → display_set_backlight()
 *   ST7789_CMD_GET_INFO     → display_get_info()
 *
 * 所有操作携带显式 timeout_ms, 不再通过 ioctl void* 传递.
 */

#endif /* ST7789_DRIVER_H */
