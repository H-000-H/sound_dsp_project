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

#ifdef __cplusplus
}
#endif

#endif /* HAL_RMT_LED_H */
