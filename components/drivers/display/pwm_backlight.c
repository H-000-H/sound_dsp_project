#include "driver.h"
#include "VFS.h"
#include "hal_pwm.h"
#include "vfs_pwm.h"
#include "osal.h"
#include <string.h>

static const char* kTag = "pwm_bl";

typedef struct {
    hal_pwm_channel_t pwm;
} pwm_bl_priv_t;

#define PWM_PRIV_POOL_SIZE 2
static pwm_bl_priv_t s_pwm_priv_pool[PWM_PRIV_POOL_SIZE];
static uint8_t s_pwm_priv_used[PWM_PRIV_POOL_SIZE];

static int pwm_bl_ioctl(device_t* dev, int cmd, void* arg)
{
    pwm_bl_priv_t* priv = (pwm_bl_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    switch (cmd) {
    case PWM_CMD_SET_DUTY:
        if (!arg) return -1;
        return priv->pwm.set_duty(&priv->pwm, *(uint32_t*)arg);

    case PWM_CMD_GET_DUTY:
        if (!arg) return -1;
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

    pwm_bl_priv_t* priv = NULL;
    for (int i = 0; i < PWM_PRIV_POOL_SIZE; i++) {
        if (!s_pwm_priv_used[i]) {
            s_pwm_priv_used[i] = 1;
            priv = &s_pwm_priv_pool[i];
            memset(priv, 0, sizeof(*priv));
            break;
        }
    }
    if (!priv) return VFS_ERR_NOMEM;

    hal_pwm_init_struct(&priv->pwm);
    int ret = priv->pwm.init(&priv->pwm, pin, freq, res);
    if (ret != 0) {
        for (int i = 0; i < PWM_PRIV_POOL_SIZE; i++) { if (&s_pwm_priv_pool[i] == priv) { s_pwm_priv_used[i] = 0; break; } }
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
        priv->pwm.deinit(&priv->pwm);
        for (int i = 0; i < PWM_PRIV_POOL_SIZE; i++) { if (&s_pwm_priv_pool[i] == priv) { s_pwm_priv_used[i] = 0; break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(pwm_bl, "generic,pwm-backlight", pwm_bl_probe, pwm_bl_remove);
