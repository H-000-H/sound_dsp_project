#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

int ws2812_init(device_t* dev);
int ws2812_set_color(device_t* dev, uint8_t r, uint8_t g, uint8_t b);
int ws2812_set_brightness(device_t* dev, uint8_t brightness);
int ws2812_off(device_t* dev);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_DRIVER_H */
