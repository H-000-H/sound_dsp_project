#ifndef __BSP_RGB_LED_H__
#define __BSP_RGB_LED_H__

#include "config.hpp"
#if CONFIG_ENABLE_BSP_RGB_LED
#include <stdint.h>
#include <driver/rmt_tx.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    rmt_channel_handle_t channel_handle;
    rmt_encoder_handle_t encoder;
    uint8_t rgb_bright;
    uint8_t cur_r;
    uint8_t cur_g;
    uint8_t cur_b;
} rgb_led_handle_t;

void rgb_led_init(rgb_led_handle_t* handle, uint32_t gpio_num);
void rgb_led_set_color(rgb_led_handle_t* handle, uint8_t red, uint8_t green, uint8_t blue);
void rgb_led_set_color_u32(rgb_led_handle_t* handle, uint32_t color);
void rgb_led_set_bright(rgb_led_handle_t* handle, uint8_t bright);
void rgb_led_off(rgb_led_handle_t* handle);

#ifdef __cplusplus
}
#endif
#endif
#endif