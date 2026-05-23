#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_engine input_engine_t;

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_PRESS,
    INPUT_EVENT_RELEASE,
    INPUT_EVENT_SHORT_PRESS,
    INPUT_EVENT_LONG_PRESS,
} input_event_t;

typedef struct {
    const char*   name;
    int           gpio_pin;
    input_event_t event;
    uint32_t      press_ms;
} input_state_t;

struct input_engine {
    int (*init)(input_engine_t* eng);
    int (*scan)(input_engine_t* eng, input_state_t* out, int max);
    int (*get_key_count)(input_engine_t* eng);
    void (*deinit)(input_engine_t* eng);
    void* _impl;
};

void input_engine_init_struct(input_engine_t* eng);

#ifdef __cplusplus
}
#endif
