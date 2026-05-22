#include "max98357a_driver.h"

#include "driver.h"
#include "hal_gpio.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "max98357a";

typedef struct {
    int sdn_pin;
    int active_level;
} max98357a_priv_t;

static int max98357a_probe(device_t* dev)
{
    int sdn_pin = -1, active = 1;
    device_get_prop_int(dev, "sdn_pin", &sdn_pin);
    device_get_prop_int(dev, "active_level", &active);

    if (sdn_pin < 0) {
        ESP_LOGE(kTag, "missing sdn_pin");
        return -1;
    }

    max98357a_priv_t* priv = (max98357a_priv_t*)calloc(1, sizeof(max98357a_priv_t));
    if (!priv) return -1;
    priv->sdn_pin = sdn_pin;
    priv->active_level = active;

    /* 初始化 SDN pin 为推挽输出 */
    hal_gpio_config_t cfg = { .pin = sdn_pin, .mode = HAL_GPIO_MODE_OUTPUT };
    hal_gpio_init(&cfg);

    /* 默认使能功放 */
    hal_gpio_set_level(sdn_pin, active);

    device_set_priv(dev, priv);
    ESP_LOGI(kTag, "probed: SDN=GPIO%d, active=%d", sdn_pin, active);
    return 0;
}

static int max98357a_remove(device_t* dev)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (priv) {
        hal_gpio_set_level(priv->sdn_pin, !priv->active_level);  /* 关闭 */
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(max98357a, "maxim,max98357a", max98357a_probe, max98357a_remove);

/* ── 公开 API ── */
int max98357a_init(device_t* dev)
{
    return 0;  /* probe 时已完成 */
}

int max98357a_set_enable(device_t* dev, int enable)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    int level = enable ? priv->active_level : !priv->active_level;
    hal_gpio_set_level(priv->sdn_pin, level);
    return 0;
}
