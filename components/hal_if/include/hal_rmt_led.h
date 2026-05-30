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
    int (*deinit)(hal_rmt_led_t* led);
    void* _impl;
};

void hal_rmt_led_init_struct(hal_rmt_led_t* led);

#define RMT_CMD_INIT         0x00
#define RMT_CMD_SET_RGB      0x01
#define RMT_CMD_SET_BRIGHT   0x02
#define RMT_CMD_OFF          0x03
#define RMT_CMD_DEINIT       0x04

typedef struct {
    int gpio;
    uint32_t resolution_hz;
} rmt_init_arg_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rmt_rgb_arg_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_RMT_LED_H */
