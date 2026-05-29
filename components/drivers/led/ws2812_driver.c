#include "ws2812_driver.h"

#include "driver.h"
#include "hal_rmt_led.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "ws2812";

typedef struct {
    hal_rmt_led_t led;
} ws2812_priv_t;

/* ── VFS 操作表 ── */
static int8_t ws2812_init(device_t* dev);
static int8_t ws2812_ioctl(device_t* dev, int cmd, void* arg);
static const file_operation_t ws2812_fops = {
    .init  = ws2812_init,
    .ioctl = ws2812_ioctl,
};

static int ws2812_probe(device_t* dev)
{
    int gpio = -1, led_count = 1, brightness = 128;
    uint32_t rmt_res = 10 * 1000 * 1000;

    device_get_prop_int(dev, "gpio", &gpio);
    device_get_prop_int(dev, "led_count", &led_count);
    device_get_prop_int(dev, "brightness", &brightness);
    device_get_prop_int(dev, "rmt_resolution_hz", (int*)&rmt_res);

    if (gpio < 0) {
        ESP_LOGE(kTag, "missing gpio");
        return -1;
    }

    ws2812_priv_t* priv = (ws2812_priv_t*)calloc(1, sizeof(ws2812_priv_t));
    if (!priv) return -1;

    hal_rmt_led_init_struct(&priv->led);
    int ret = priv->led.init(&priv->led, gpio, rmt_res);
    if (ret != 0) {
        free(priv);
        return ret;
    }

    priv->led.set_brightness(&priv->led, (uint8_t)brightness);
    priv->led.off(&priv->led);

    device_set_priv(dev, priv);
    dev->ops = &ws2812_fops;
    ESP_LOGI(kTag, "probed: GPIO=%d, count=%d, brightness=%d", gpio, led_count, brightness);
    return 0;
}

static int ws2812_remove(device_t* dev)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->led.off(&priv->led);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(ws2812, "worldsemi,ws2812", ws2812_probe, ws2812_remove);

/* ── 公开 API ── */
static int8_t ws2812_init(device_t* dev)
{
    return 0;
}

static int ws2812_set_color(device_t* dev, uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->led.set_rgb(&priv->led, r, g, b);
}

static int ws2812_set_brightness(device_t* dev, uint8_t brightness)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->led.set_brightness(&priv->led, brightness);
}

static int ws2812_off(device_t* dev)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->led.off(&priv->led);
}

/* ── ioctl ── */
static int8_t ws2812_ioctl(device_t* dev, int cmd, void* arg)
{
    switch (cmd) {
    case WS2812_CMD_SET_COLOR:
        if (!arg) return -1;
        {
            ws2812_color_t* c = (ws2812_color_t*)arg;
            return ws2812_set_color(dev, c->r, c->g, c->b);
        }
    case WS2812_CMD_SET_BRIGHTNESS:
        if (!arg) return -1;
        return ws2812_set_brightness(dev, *(uint8_t*)arg);
    case WS2812_CMD_OFF:
        return ws2812_off(dev);
    default:
        return -1;
    }
}
