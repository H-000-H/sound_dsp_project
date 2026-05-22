#ifndef GPIO_KEY_DRIVER_H
#define GPIO_KEY_DRIVER_H

#include <stdint.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 按键事件类型 ── */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,
    KEY_EVENT_RELEASE,
    KEY_EVENT_SHORT_PRESS,
    KEY_EVENT_LONG_PRESS,
} gpio_key_event_t;

/* ── 按键状态 ── */
typedef struct {
    const char*   name;      /* "next", "prev", "enter", "esc" */
    int           gpio_pin;
    gpio_key_event_t event;
    uint32_t      press_ms;  /* 按下持续时间 */
} gpio_key_state_t;

/* ── API ── */
int gpio_key_init(device_t* dev);
int gpio_key_scan(device_t* dev, gpio_key_state_t* out, int max_keys);
int gpio_key_get_count(device_t* dev);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_KEY_DRIVER_H */
