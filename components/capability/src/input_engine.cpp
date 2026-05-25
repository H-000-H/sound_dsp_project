#include "capability/input_engine.hpp"

#include "device.h"
#include "gpio_key_driver.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "input_engine";

typedef struct 
{
    device_t* dev;
} input_engine_impl_t;

static int eng_init(input_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    input_engine_impl_t* impl = (input_engine_impl_t*)eng->_impl;
    impl->dev = device_find("buttons0");
    if (!impl->dev)
    {
        ESP_LOGE(kTag, "buttons0 not found");
        return -1;
    }
    return gpio_key_init(impl->dev);
}

static int eng_scan(input_engine_t* eng, input_state_t* out, int max)
{
    if (!eng || !eng->_impl || !out || max < 1) return -1;
    input_engine_impl_t* impl = (input_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return gpio_key_scan(impl->dev, (gpio_key_state_t*)out, max);
}

static int eng_get_key_count(input_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    input_engine_impl_t* impl = (input_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return gpio_key_get_count(impl->dev);
}

static void eng_deinit(input_engine_t* eng)
{
    if (!eng || !eng->_impl) return;
    free(eng->_impl);
    eng->_impl = NULL;
}

void input_engine_init_struct(input_engine_t* eng)
{
    if (!eng) return;
    eng->init = eng_init;
    eng->scan = eng_scan;
    eng->get_key_count = eng_get_key_count;
    eng->deinit = eng_deinit;
    eng->_impl = calloc(1, sizeof(input_engine_impl_t));
}
