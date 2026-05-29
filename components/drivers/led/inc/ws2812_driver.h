#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── ioctl 命令 ── */
#define WS2812_CMD_SET_COLOR      1  /* arg: ws2812_color_t* */
#define WS2812_CMD_SET_BRIGHTNESS 2  /* arg: uint8_t* */
#define WS2812_CMD_OFF            3  /* arg: NULL */

typedef struct {
    uint8_t r, g, b;
} ws2812_color_t;

#ifdef __cplusplus
}
#endif

#endif /* WS2812_DRIVER_H */
