#include "max98357a_driver.h"
#include "driver.h"
#include "vfs_gpio.h"
#include "osal.h"
#include <string.h>

static const char* kTag = "max98357a";

typedef struct
{
    device_t* gpio_dev;
    int sdn_pin;
    int active_level;
} max98357a_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define MAX98357A_PRIV_POOL_SIZE 2
static max98357a_priv_t s_max98357a_pool[MAX98357A_PRIV_POOL_SIZE];
static uint8_t s_max98357a_used[MAX98357A_PRIV_POOL_SIZE];

/* ── VFS 操作表 ── */
static int max98357a_init(device_t* dev);
static int max98357a_ioctl(device_t* dev, int cmd, void* arg);
static const file_operation_t max98357a_fops = {
    .init  = max98357a_init,
    .ioctl = max98357a_ioctl,
};

static int max98357a_probe(device_t* dev)
{
    int sdn_pin = 0, active = 0;
    device_get_prop_int(dev, "sdn_pin", &sdn_pin);
    device_get_prop_int(dev, "active_level", &active);

    if (sdn_pin < 0) {
        DRV_LOGE(kTag, "missing sdn_pin");
        return -1;
    }

    max98357a_priv_t* priv = NULL;
    for (int i = 0; i < MAX98357A_PRIV_POOL_SIZE; i++) {
        if (!s_max98357a_used[i]) {
            s_max98357a_used[i] = 1;
            priv = &s_max98357a_pool[i];
            memset(priv, 0, sizeof(*priv));
            break;
        }
    }
    if (!priv)
    {
        return -1;
    }

    int ret = 0;
    priv->gpio_dev = device_get_phandle_dev(dev, "gpio");
    if (!priv->gpio_dev) {
        ret = -1;
        goto err_pool;
    }
    priv->sdn_pin = sdn_pin;
    priv->active_level = active;

    hal_gpio_config_t cfg = { .pin = sdn_pin, .mode = HAL_GPIO_MODE_OUTPUT };
    ret = device_ioctl(priv->gpio_dev, GPIO_CMD_CONFIG, &cfg);
    if (ret != 0) {
        goto err_pool;
    }

    gpio_level_arg_t level = { .pin = sdn_pin, .level = active };
    ret = device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level);
    if (ret != 0) {
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &max98357a_fops;
    DRV_LOGI(kTag, "probed: SDN=GPIO%d, active=%d", sdn_pin, active);
    return 0;

err_pool:
    for (int i = 0; i < MAX98357A_PRIV_POOL_SIZE; i++) { if (&s_max98357a_pool[i] == priv) { s_max98357a_used[i] = 0; break; } }
    return ret;
}

static int max98357a_remove(device_t* dev)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (priv) {
        gpio_level_arg_t level = { .pin = priv->sdn_pin, .level = !priv->active_level };
        device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level);
        for (int i = 0; i < MAX98357A_PRIV_POOL_SIZE; i++) { if (&s_max98357a_pool[i] == priv) { s_max98357a_used[i] = 0; break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(max98357a, "maxim,max98357a", max98357a_probe, max98357a_remove);

/* ── 内部 API（仅通过 ops 表调用） ── */
static int max98357a_init(device_t* dev)
{
    (void)dev;
    return 0;
}

static int max98357a_set_enable(device_t* dev, int enable)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    int level = enable ? priv->active_level : !priv->active_level;
    gpio_level_arg_t arg = { .pin = priv->sdn_pin, .level = level };
    return device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &arg);
}

static int max98357a_ioctl(device_t* dev, int cmd, void* arg)
{
    switch (cmd) {
    case MAX98357A_CMD_SET_ENABLE:
        if (!arg) return -1;
        return max98357a_set_enable(dev, *(int*)arg);
    default:
        return -1;
    }
}
