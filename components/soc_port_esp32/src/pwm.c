#include "hal_pwm.h"

#include "driver/ledc.h"
#include "osal.h"
#include "VFS.h"
#include <string.h>

static const char* kTag = "hal_pwm";

typedef struct {
    ledc_channel_t channel;
    ledc_mode_t speed_mode;
} hal_pwm_impl_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define PWM_IMPL_POOL_SIZE 8
static hal_pwm_impl_t s_pwm_pool[PWM_IMPL_POOL_SIZE];
static uint8_t s_pwm_used[PWM_IMPL_POOL_SIZE];

static int pwm_init_impl(hal_pwm_channel_t* pwm, int pin, int freq_hz, int resolution_bits)
{
    if (pwm == NULL || pin < 0 || freq_hz <= 0 || resolution_bits <= 0 || resolution_bits > 14) {
        return -1;
    }

    hal_pwm_impl_t* impl = NULL;
    for (int i = 0; i < PWM_IMPL_POOL_SIZE; i++) {
        if (!s_pwm_used[i]) {
            s_pwm_used[i] = 1;
            impl = &s_pwm_pool[i];
            memset(impl, 0, sizeof(*impl));
            break;
        }
    }
    if (!impl) {
        DRV_LOGE(kTag, "impl pool exhausted");
        return VFS_ERR_NOMEM;
    }

    int ret = 0;
    static int next_timer = 0;
    static int next_channel = 0;
    ledc_timer_t timer = LEDC_TIMER_0 + (next_timer % 4);
    ledc_channel_t channel = LEDC_CHANNEL_0 + (next_channel % 8);
    next_timer++;
    next_channel++;

    ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;

    ledc_timer_config_t timer_cfg = {
        .speed_mode = speed_mode,
        .duty_resolution = resolution_bits,
        .timer_num = timer,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t esp_ret = ledc_timer_config(&timer_cfg);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "ledc_timer_config failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_pool;
    }

    ledc_channel_config_t chan_cfg = {
        .gpio_num = pin,
        .speed_mode = speed_mode,
        .channel = channel,
        .timer_sel = timer,
        .duty = 0,
        .hpoint = 0,
    };

    esp_ret = ledc_channel_config(&chan_cfg);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "ledc_channel_config failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_pool;
    }

    impl->channel = channel;
    impl->speed_mode = speed_mode;
    pwm->_impl = impl;
    DRV_LOGI(kTag, "PWM init: pin=%d freq=%d res=%d timer=%d chan=%d",
             pin, freq_hz, resolution_bits, timer, channel);
    return 0;

err_pool:
    for (int i = 0; i < PWM_IMPL_POOL_SIZE; i++) { if (&s_pwm_pool[i] == impl) { s_pwm_used[i] = 0; break; } }
    return ret;
}

static int pwm_set_duty_impl(hal_pwm_channel_t* pwm, uint32_t duty)
{
    if (pwm == NULL || pwm->_impl == NULL) {
        return -1;
    }

    hal_pwm_impl_t* impl = (hal_pwm_impl_t*)pwm->_impl;
    esp_err_t ret = ledc_set_duty_and_update(impl->speed_mode, impl->channel, duty, 0);
    return (ret == ESP_OK) ? 0 : (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
}

static int pwm_get_duty_impl(hal_pwm_channel_t* pwm, uint32_t* duty)
{
    if (pwm == NULL || pwm->_impl == NULL || duty == NULL) {
        return -1;
    }
    hal_pwm_impl_t* impl = (hal_pwm_impl_t*)pwm->_impl;
    *duty = ledc_get_duty(impl->speed_mode, impl->channel);
    return 0;
}

static int pwm_deinit_impl(hal_pwm_channel_t* pwm)
{
    if (pwm == NULL || pwm->_impl == NULL) {
        return -1;
    }

    hal_pwm_impl_t* impl = (hal_pwm_impl_t*)pwm->_impl;
    ledc_stop(impl->speed_mode, impl->channel, 0);
    for (int i = 0; i < PWM_IMPL_POOL_SIZE; i++) { if (&s_pwm_pool[i] == impl) { s_pwm_used[i] = 0; break; } }
    pwm->_impl = NULL;
    return 0;
}

void hal_pwm_init_struct(hal_pwm_channel_t* pwm)
{
    if (pwm == NULL) return;
    pwm->init = pwm_init_impl;
    pwm->set_duty = pwm_set_duty_impl;
    pwm->get_duty = pwm_get_duty_impl;
    pwm->deinit = pwm_deinit_impl;
    pwm->_impl = NULL;
}

#include "driver.h"

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
        DRV_LOGE(kTag, "pwm_bl: missing pwm_pin");
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
    DRV_LOGI(kTag, "pwm_bl probed: pin=%d freq=%d res=%d", pin, freq, res);
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

DRIVER_REGISTER(pwm_bl, "esp32,pwm-backlight", pwm_bl_probe, pwm_bl_remove);
