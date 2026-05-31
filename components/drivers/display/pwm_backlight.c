#include "driver.h"
#include "VFS.h"
#include "hal_pwm.h"
#include "vfs_pwm.h"
#include "osal.h"
#include <string.h>
#include "board_config.h"

static const char* kTag = "pwm_bl";

typedef struct {
    hal_pwm_channel_t pwm;
    int pool_idx;
} pwm_bl_priv_t;

static pwm_bl_priv_t s_pwm_priv_pool[PWM_BACKLIGHT_COUNT];
static uint8_t s_pwm_priv_used[PWM_BACKLIGHT_COUNT];

static int pwm_bl_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    pwm_bl_priv_t* priv = (pwm_bl_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    switch (cmd) {
    case PWM_CMD_SET_DUTY:
        if (arg_len != sizeof(uint32_t) || !arg) return VFS_ERR_INVAL;
        return priv->pwm.set_duty(&priv->pwm, *(uint32_t*)arg);

    case PWM_CMD_GET_DUTY:
        if (arg_len != sizeof(uint32_t) || !arg) return VFS_ERR_INVAL;
        return priv->pwm.get_duty(&priv->pwm, (uint32_t*)arg);

    case PWM_CMD_DEINIT:
        return priv->pwm.deinit(&priv->pwm);

    default:
        return -1;
    }
}

static const file_operation_t pwm_bl_fops = {
    .ioctl = pwm_bl_ioctl,
};

static int pwm_bl_probe(device_t* dev)
{
    int pin = -1, freq = 5000, res = 10;

    device_get_prop_int(dev, "pwm_pin", &pin);
    device_get_prop_int(dev, "freq_hz", &freq);
    device_get_prop_int(dev, "resolution_bits", &res);

    if (pin < 0) {
        DRV_LOGE(kTag, "missing pwm_pin");
        return -1;
    }

    int pool_idx = osal_pool_claim(s_pwm_priv_used, PWM_BACKLIGHT_COUNT);
    if (pool_idx < 0) return VFS_ERR_NOMEM;
    pwm_bl_priv_t* priv = &s_pwm_priv_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->pool_idx = pool_idx;

    hal_pwm_init_struct(&priv->pwm);
    int ret = priv->pwm.init(&priv->pwm, pin, freq, res);
    if (ret != 0) {
        osal_pool_release(s_pwm_priv_used, PWM_BACKLIGHT_COUNT, pool_idx);
        return ret;
    }

    device_set_priv(dev, priv);
    dev->ops = &pwm_bl_fops;
    DRV_LOGI(kTag, "probed: pin=%d freq=%d res=%d", pin, freq, res);
    return 0;
}

static int pwm_bl_remove(device_t* dev)
{
    pwm_bl_priv_t* priv = (pwm_bl_priv_t*)device_get_priv(dev);
    if (priv) {
        uint32_t safe_duty = 0;
        (void)priv->pwm.set_duty(&priv->pwm, safe_duty);
        priv->pwm.deinit(&priv->pwm);
        osal_pool_release(s_pwm_priv_used, PWM_BACKLIGHT_COUNT, priv->pool_idx);
        device_ops_unregister(dev);
    }
    return 0;
}

DRIVER_REGISTER(pwm_bl, "generic,pwm-backlight", pwm_bl_probe, pwm_bl_remove);
