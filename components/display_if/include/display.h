#ifndef DISPLAY_IF_H
#define DISPLAY_IF_H

#include <stdint.h>
#include <stddef.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 显示设备信息 ── */
typedef struct {
    int width;
    int height;
} display_info_t;

/* ── 显示操作表 (驱动 probe 时注入 priv_data 魔术头) ── */
struct display_ops {
    int (*fill_rect)(device_t* dev, int x, int y, int w, int h, uint16_t color, uint32_t timeout_ms);
    int (*fill_screen)(device_t* dev, uint16_t color, uint32_t timeout_ms);
    int (*draw_bitmap)(device_t* dev, int x, int y, int w, int h, const uint8_t* data, uint32_t timeout_ms);
    int (*write_ram)(device_t* dev, int x, int y, int w, int h, const uint16_t* pixels, uint32_t timeout_ms);
    int (*set_backlight)(device_t* dev, uint8_t brightness);
    int (*get_info)(device_t* dev, display_info_t* info);
};

#define DISPLAY_IF_MAGIC 0x44504C59U  /* "DPLY" */

/* ── 显示驱动 priv_data 魔术头 ──
 * 通过 device_set_subsys_priv() 显式绑定, 不依赖结构体偏移.
 * magic 做运行时类型鉴别, 避免在全局 device_t 中硬编码字段.
 */
typedef struct {
    uint32_t             magic;
    const struct display_ops* ops;
} display_if_priv_t;

/* ── 强类型显示 API ──
 * 通过 dev->priv_data 中提取 ops 表分发.
 * 不加锁 — 锁定策略由驱动层的 ops 回调自行管理.
 */
int display_fill_rect(device_t* dev, int x, int y, int w, int h, uint16_t color, uint32_t timeout_ms);
int display_fill_screen(device_t* dev, uint16_t color, uint32_t timeout_ms);
int display_draw_bitmap(device_t* dev, int x, int y, int w, int h, const uint8_t* data, uint32_t timeout_ms);
int display_write_ram(device_t* dev, int x, int y, int w, int h, const uint16_t* pixels, uint32_t timeout_ms);
int display_set_backlight(device_t* dev, uint8_t brightness);
int display_get_info(device_t* dev, display_info_t* info);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_IF_H */
