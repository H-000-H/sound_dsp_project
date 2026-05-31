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
    return device_open(impl->dev, NULL);
}

static int eng_set_color(led_engine_t* eng, uint8_t r, uint8_t g, uint8_t b)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    ws2812_color_t c = { .r = r, .g = g, .b = b };
    return device_ioctl(impl->dev, WS2812_CMD_SET_COLOR, &c, sizeof(c), 500);
}

static int eng_set_brightness(led_engine_t* eng, uint8_t brightness)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return device_ioctl(impl->dev, WS2812_CMD_SET_BRIGHTNESS, &brightness, sizeof(brightness), 500);
}

static int eng_off(led_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    led_engine_impl_t* impl = (led_engine_impl_t*)eng->_impl;
    if (!impl->dev) return -1;
    return device_ioctl(impl->dev, WS2812_CMD_OFF, NULL, 0, 500);
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
