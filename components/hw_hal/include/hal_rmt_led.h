#ifndef HAL_RMT_LED_H
#define HAL_RMT_LED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_rmt_led hal_rmt_led_t;

struct hal_rmt_led 
{
    int (*init)(hal_rmt_led_t* led, int gpio_num, uint32_t resolution_hz);
    int (*set_rgb)(hal_rmt_led_t* led, uint8_t r, uint8_t g, uint8_t b);
    int (*set_brightness)(hal_rmt_led_t* led, uint8_t brightness);
    int (*off)(hal_rmt_led_t* led);
    void* _impl;
};

void hal_rmt_led_init_struct(hal_rmt_led_t* led);

/* ── ioctl 命令 (平台驱动模式) ── */
#define RMT_CMD_SET_RGB      0x01  /* arg: rmt_rgb_arg_t* */
#define RMT_CMD_SET_BRIGHT   0x02  /* arg: uint8_t* */
#define RMT_CMD_OFF          0x03  /* arg: NULL */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rmt_rgb_arg_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_RMT_LED_H */
