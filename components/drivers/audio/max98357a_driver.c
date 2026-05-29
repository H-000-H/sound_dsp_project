#include "max98357a_driver.h"
#include "driver.h"
#include "hal_gpio.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "max98357a";

typedef struct
{
    int sdn_pin;
    int active_level;
} max98357a_priv_t;

/* ── VFS 操作表 ── */
static int8_t max98357a_init(device_t* dev);
static int8_t max98357a_ioctl(device_t* dev, int cmd, void* arg);
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
        ESP_LOGE(kTag, "missing sdn_pin");
        return -1;
    }

    int ret = 0;
    max98357a_priv_t* priv = (max98357a_priv_t*)calloc(1, sizeof(max98357a_priv_t));
    if (!priv)
    {
        return ret;
    }
    priv->sdn_pin = sdn_pin;
    priv->active_level = active;

    hal_gpio_config_t cfg = { .pin = sdn_pin, .mode = HAL_GPIO_MODE_OUTPUT };
    hal_gpio_init(&cfg);

    hal_gpio_set_level(sdn_pin, active);

    device_set_priv(dev, priv);
    dev->ops = &max98357a_fops;
    ESP_LOGI(kTag, "probed: SDN=GPIO%d, active=%d", sdn_pin, active);
    return 0;
}

static int max98357a_remove(device_t* dev)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (priv) {
        hal_gpio_set_level(priv->sdn_pin, !priv->active_level);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(max98357a, "maxim,max98357a", max98357a_probe, max98357a_remove);

/* ── 内部 API（仅通过 ops 表调用） ── */
static int8_t max98357a_init(device_t* dev)
{
    (void)dev;
    return 0;
}

static int max98357a_set_enable(device_t* dev, int enable)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    int level = enable ? priv->active_level : !priv->active_level;
    hal_gpio_set_level(priv->sdn_pin, level);
    return 0;
}

static int8_t max98357a_ioctl(device_t* dev, int cmd, void* arg)
{
    switch (cmd) {
    case MAX98357A_CMD_SET_ENABLE:
        if (!arg) return -1;
        return max98357a_set_enable(dev, *(int*)arg);
    default:
        return -1;
    }
}
