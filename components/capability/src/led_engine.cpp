#include "capability/led_engine.hpp"

#include "device.h"
#include "ws2812_driver.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "led_engine";

typedef struct {
    device_t* dev;
} led_engine_impl_t;

static int eng_init(led_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    impl->dev = device_find("rgb_led0");
    if (!impl->dev)
    {
        ESP_LOGE(kTag, "rgb_led0 not found");
        return -1;
    }
    return ws2812_init(impl->dev);
}

static int eng_set_color(led_engine_t* eng, uint8_t r, uint8_t g, uint8_t b)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return ws2812_set_color(impl->dev, r, g, b);
}

static int eng_set_brightness(led_engine_t* eng, uint8_t brightness)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return ws2812_set_brightness(impl->dev, brightness);
}

static int eng_off(led_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return ws2812_off(impl->dev);
}

static void eng_deinit(led_engine_t* eng)
{
    if (!eng || !eng->_impl) return;
    free(eng->_impl);
    eng->_impl = NULL;
}

void led_engine_init_struct(led_engine_t* eng)
{
    if (!eng) return;
    eng->init = eng_init;
    eng->set_color = eng_set_color;
    eng->set_brightness = eng_set_brightness;
    eng->off = eng_off;
    eng->deinit = eng_deinit;
    eng->_impl = calloc(1, sizeof(led_engine_impl_t));
}
