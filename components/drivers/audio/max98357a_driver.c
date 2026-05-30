#include "max98357a_driver.h"
#include "driver.h"

#include "VFS.h"
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
static int max98357a_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len);
static int max98357a_suspend(device_t* dev);
static int max98357a_resume(device_t* dev);
static const file_operation_t max98357a_fops = {
    .init    = max98357a_init,
    .ioctl   = max98357a_ioctl,
    .suspend = max98357a_suspend,
    .resume  = max98357a_resume,
};

static int max98357a_probe(device_t* dev)
{
    int sdn_pin = 0, active = 0;
    device_get_prop_int(dev, "sdn_pin", &sdn_pin);
    device_get_prop_int(dev, "active_level", &active);

    int ret = 0;
    max98357a_priv_t* priv = NULL;

    if (sdn_pin < 0) {
        DRV_LOGE(kTag, "missing sdn_pin");
        ret = VFS_ERR_INVAL;
        goto err_pool;
    }

    int pool_idx = osal_pool_claim(s_max98357a_used, MAX98357A_PRIV_POOL_SIZE);
    if (pool_idx < 0) {
        ret = VFS_ERR_NOMEM;
        goto err_pool;
    }
    priv = &s_max98357a_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->gpio_dev = device_get_phandle_dev(dev, "gpio");
    if (!priv->gpio_dev) {
        ret = VFS_ERR_IO;
        goto err_pool;
    }
    priv->sdn_pin = sdn_pin;
    priv->active_level = active;

    hal_gpio_config_t cfg = { .pin = sdn_pin, .mode = HAL_GPIO_MODE_OUTPUT };
    ret = device_ioctl(priv->gpio_dev, GPIO_CMD_CONFIG, &cfg, sizeof(cfg));
    if (ret != 0) {
        goto err_pool;
    }

    gpio_level_arg_t level = { .pin = sdn_pin, .level = active };
    ret = device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level, sizeof(level));
    if (ret != 0) {
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &max98357a_fops;
    DRV_LOGI(kTag, "probed: SDN=GPIO%d, active=%d", sdn_pin, active);
    return 0;

err_pool:
    if (priv) {
        for (int i = 0; i < MAX98357A_PRIV_POOL_SIZE; i++) { if (&s_max98357a_pool[i] == priv) { osal_pool_release(s_max98357a_used, MAX98357A_PRIV_POOL_SIZE, i); break; } }
    }
    return ret;
}

static int max98357a_remove(device_t* dev)
{
    max98357a_priv_t* priv = (max98357a_priv_t*)device_get_priv(dev);
    if (priv) {
        gpio_level_arg_t level = { .pin = priv->sdn_pin, .level = !priv->active_level };
        device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level, sizeof(level));
        for (int i = 0; i < MAX98357A_PRIV_POOL_SIZE; i++) { if (&s_max98357a_pool[i] == priv) { osal_pool_release(s_max98357a_used, MAX98357A_PRIV_POOL_SIZE, i); break; } }
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
    if (!priv) return VFS_ERR_INVAL;

    int level = enable ? priv->active_level : !priv->active_level;
    gpio_level_arg_t arg = { .pin = priv->sdn_pin, .level = level };
    return device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &arg, sizeof(arg));
}

static int max98357a_suspend(device_t* dev)
{
    /* 系统休眠: 关闭功放 (SDN = inactive), 时序必须先于 I2S 时钟关闭 */
    DRV_LOGD(kTag, "suspend: disabling amplifier");
    return max98357a_set_enable(dev, 0);
}

static int max98357a_resume(device_t* dev)
{
    /* 系统唤醒: 开启功放 (SDN = active), 时序必须在 I2S 时钟恢复之后 */
    DRV_LOGD(kTag, "resume: enabling amplifier");
    return max98357a_set_enable(dev, 1);
}

static int max98357a_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)
{
    switch (cmd) {
    case MAX98357A_CMD_SET_ENABLE:
        if (!arg) return VFS_ERR_INVAL;
        return max98357a_set_enable(dev, *(int*)arg);
    default:
        return VFS_ERR_INVAL;
    }
}
