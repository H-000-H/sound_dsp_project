#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct led_engine led_engine_t;

struct led_engine {
    int (*init)(led_engine_t* eng);
    int (*set_color)(led_engine_t* eng, uint8_t r, uint8_t g, uint8_t b);
    int (*set_brightness)(led_engine_t* eng, uint8_t brightness);
    int (*off)(led_engine_t* eng);
    void (*deinit)(led_engine_t* eng);
    void* _impl;
};

void led_engine_init_struct(led_engine_t* eng);

#ifdef __cplusplus
}
#endif
