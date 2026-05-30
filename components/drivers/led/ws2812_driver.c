#include "ws2812_driver.h"

#include "driver.h"
#include "VFS.h"
#include "osal.h"
#include <string.h>

static const char* kTag = "ws2812";

typedef struct {
    device_t* parent;    /* RMT parent device (发送颜色数据) */
    uint8_t*  rgb_buf;   /* platform_data: board 层静态 RGB 缓冲区, RGB order */
    uint8_t   brightness;
} ws2812_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define WS2812_PRIV_POOL_SIZE 2
static ws2812_priv_t s_ws2812_pool[WS2812_PRIV_POOL_SIZE];
static uint8_t s_ws2812_used[WS2812_PRIV_POOL_SIZE];

static int ws2812_init(device_t* dev);
static int ws2812_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len);
static const file_operation_t ws2812_fops = {
    .init  = ws2812_init,
    .ioctl = ws2812_ioctl,
};

static int ws2812_probe(device_t* dev)
{
    int led_count = 1, brightness = 28;
    device_get_prop_int(dev, "led_count", &led_count);
    device_get_prop_int(dev, "brightness", &brightness);

    int ret = 0;
    ws2812_priv_t* priv = NULL;

    if (led_count != 1) {
        DRV_LOGE(kTag, "led_count must be 1");
        ret = VFS_ERR_INVAL;
        goto err_pool;
    }

    int pool_idx = osal_pool_claim(s_ws2812_used, WS2812_PRIV_POOL_SIZE);
    if (pool_idx < 0) {
        ret = VFS_ERR_NOMEM;
        goto err_pool;
    }
    priv = &s_ws2812_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));

    priv->brightness = (uint8_t)brightness;
    priv->rgb_buf = (uint8_t*)dev->platform_data;
    priv->parent = device_get_parent(dev);

    if (!priv->parent) {
        DRV_LOGE(kTag, "no parent (RMT) device");
        ret = VFS_ERR_IO;
        goto err_pool;
    }

    /* 初始关闭: 发送全零 */
    uint8_t grb[3] = {0, 0, 0};
    ret = device_write(priv->parent, grb, 3);
    if (ret != 0) {
        DRV_LOGW(kTag, "initial off failed: %d", ret);
    }

    device_set_priv(dev, priv);
    dev->ops = &ws2812_fops;
    DRV_LOGI(kTag, "probed: count=%d, brightness=%d", led_count, brightness);
    return 0;

err_pool:
    if (priv) {
        for (int i = 0; i < WS2812_PRIV_POOL_SIZE; i++) {
            if (&s_ws2812_pool[i] == priv) { osal_pool_release(s_ws2812_used, WS2812_PRIV_POOL_SIZE, i); break; }
        }
    }
    return ret;
}

static int ws2812_remove(device_t* dev)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (priv) {
        /* 关闭 LED */
        if (priv->parent) {
            uint8_t grb[3] = {0, 0, 0};
            device_write(priv->parent, grb, 3);
        }
        for (int i = 0; i < WS2812_PRIV_POOL_SIZE; i++) {
            if (&s_ws2812_pool[i] == priv) { osal_pool_release(s_ws2812_used, WS2812_PRIV_POOL_SIZE, i); break; }
        }
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
    if (!priv || !priv->parent) return VFS_ERR_INVAL;

    /* 亮度缩放 */
    uint8_t wr = (r * priv->brightness) / 255;
    uint8_t wg = (g * priv->brightness) / 255;
    uint8_t wb = (b * priv->brightness) / 255;

    /* 写入 platform_data 缓冲区 (RGB order, 供外部查询当前颜色) */
    if (priv->rgb_buf) {
        priv->rgb_buf[0] = wr;
        priv->rgb_buf[1] = wg;
        priv->rgb_buf[2] = wb;
    }

    /* GRB order = WS2812 物理协议原生格式 */
    uint8_t grb[3] = {wg, wr, wb};
    return device_write(priv->parent, grb, 3);
}

static int ws2812_set_brightness(device_t* dev, uint8_t brightness)
{
    ws2812_priv_t* priv = (ws2812_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    priv->brightness = brightness;
    return 0;
}

static int ws2812_off(device_t* dev)
{
    return ws2812_set_color(dev, 0, 0, 0);
}

static int ws2812_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)
{
    if (!dev) return VFS_ERR_INVAL;

    int ret = device_lock(dev);
    if (ret != 0) return ret;

    switch (cmd) {
    case WS2812_CMD_SET_COLOR:
        if (!arg) { ret = VFS_ERR_INVAL; break; }
        {
            ws2812_color_t* c = (ws2812_color_t*)arg;
            ret = ws2812_set_color(dev, c->r, c->g, c->b);
        }
        break;
    case WS2812_CMD_SET_BRIGHTNESS:
        if (!arg) { ret = VFS_ERR_INVAL; break; }
        ret = ws2812_set_brightness(dev, *(uint8_t*)arg);
        break;
    case WS2812_CMD_OFF:
        ret = ws2812_off(dev);
        break;
    default:
        ret = VFS_ERR_INVAL;
        break;
    }

    device_unlock(dev);
    return ret;
}
