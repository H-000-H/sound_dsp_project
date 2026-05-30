#include "ws2812_driver.h"

#include "driver.h"
#include "vfs_rmt.h"
#include "osal.h"
#include <string.h>

static const char* kTag = "ws2812";

typedef struct {
    device_t* rmt_dev;
} ws2812_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define WS2812_PRIV_POOL_SIZE 2
static ws2812_priv_t s_ws2812_pool[WS2812_PRIV_POOL_SIZE];
static uint8_t s_ws2812_used[WS2812_PRIV_POOL_SIZE];

static int ws2812_init(device_t* dev);
static int ws2812_ioctl(device_t* dev, int cmd, void* arg);
static const file_operation_t ws2812_fops = {
    .init  = ws2812_init,
    .ioctl = ws2812_ioctl,
};

static int ws2812_probe(device_t* dev)
{
    int gpio = -1, led_count = 1, brightness = 128;
    uint32_t rmt_res = 10U * 1000U * 1000U;

    device_get_prop_int(dev, "gpio", &gpio);
    device_get_prop_int(dev, "led_count", &led_count);
    device_get_prop_int(dev, "brightness", &brightness);
    device_get_prop_int(dev, "rmt_resolution_hz", (int*)&rmt_res);

    if (gpio < 0 || led_count != 1) {
        DRV_LOGE(kTag, "invalid gpio or led_count");
        return -1;
    }

    ws2812_priv_t* priv = NULL;
    for (int i = 0; i < WS2812_PRIV_POOL_SIZE; i++) {
        if (!s_ws2812_used[i]) {
            s_ws2812_used[i] = 1;
            priv = &s_ws2812_pool[i];
            memset(priv, 0, sizeof(*priv));
            break;
        }
    }
    if (!priv) return -1;

    int ret = 0;
    priv->rmt_dev = device_get_parent(dev);
    if (!priv->rmt_dev) {
        ret = -1;
        goto err_pool;
    }

    rmt_init_arg_t init_arg = { .gpio = gpio, .resolution_hz = rmt_res };
    ret = device_ioctl(priv->rmt_dev, RMT_CMD_INIT, &init_arg);
    if (ret != 0) {
        goto err_pool;
    }

    uint8_t b = (uint8_t)brightness;
    ret = device_ioctl(priv->rmt_dev, RMT_CMD_SET_BRIGHT, &b);
    if (ret == 0) ret = device_ioctl(priv->rmt_dev, RMT_CMD_OFF, NULL);
    if (ret != 0) {
        device_ioctl(priv->rmt_dev, RMT_CMD_DEINIT, NULL);
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &ws2812_fops;
    DRV_LOGI(kTag, "probed: GPIO=%d, count=%d, brightness=%d", gpio, led_count, brightness);
    return 0;

err_pool:
    for (int i = 0; i < WS2812_PRIV_POOL_SIZE; i++) { if (&s_ws2812_pool[i] == priv) { s_ws2812_used[i] = 0; break; } }
    return ret;
}

static int ws2812_remove(device_t* dev)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (priv) {
        device_ioctl(priv->rmt_dev, RMT_CMD_OFF, NULL);
        device_ioctl(priv->rmt_dev, RMT_CMD_DEINIT, NULL);
        for (int i = 0; i < WS2812_PRIV_POOL_SIZE; i++) { if (&s_ws2812_pool[i] == priv) { s_ws2812_used[i] = 0; break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(ws2812, "worldsemi,ws2812", ws2812_probe, ws2812_remove);

static int ws2812_init(device_t* dev)
{
    (void)dev;
    return 0;
}

static int ws2812_set_color(device_t* dev, uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    rmt_rgb_arg_t color = { .r = r, .g = g, .b = b };
    return device_ioctl(priv->rmt_dev, RMT_CMD_SET_RGB, &color);
}

static int ws2812_set_brightness(device_t* dev, uint8_t brightness)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return device_ioctl(priv->rmt_dev, RMT_CMD_SET_BRIGHT, &brightness);
}

static int ws2812_off(device_t* dev)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return device_ioctl(priv->rmt_dev, RMT_CMD_OFF, NULL);
}

static int ws2812_ioctl(device_t* dev, int cmd, void* arg)
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
