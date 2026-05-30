#include "hal_pwm.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char* kTag = "hal_pwm";

typedef struct {
    ledc_channel_t channel;
    ledc_mode_t speed_mode;
} hal_pwm_impl_t;

static int pwm_init_impl(hal_pwm_channel_t* pwm, int pin, int freq_hz, int resolution_bits)
{
    if (pwm == NULL) {
        return -1;
    }

    hal_pwm_impl_t* impl = (hal_pwm_impl_t*)calloc(1, sizeof(hal_pwm_impl_t));
    if (impl == NULL) {
        ESP_LOGE(kTag, "malloc failed");
        return -1;
    }

    /* Auto-select timer and channel */
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

    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "ledc_timer_config failed: %d", ret);
        free(impl);
        return ret;
    }

    ledc_channel_config_t chan_cfg = {
        .gpio_num = pin,
        .speed_mode = speed_mode,
        .channel = channel,
        .timer_sel = timer,
        .duty = 0,
        .hpoint = 0,
    };

    ret = ledc_channel_config(&chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "ledc_channel_config failed: %d", ret);
        free(impl);
        return ret;
    }

    impl->channel = channel;
    impl->speed_mode = speed_mode;
    pwm->_impl = impl;
    ESP_LOGI(kTag, "PWM init: pin=%d freq=%d res=%d timer=%d chan=%d",
             pin, freq_hz, resolution_bits, timer, channel);
    return 0;
}

static int pwm_set_duty_impl(hal_pwm_channel_t* pwm, uint32_t duty)
{
    if (pwm == NULL || pwm->_impl == NULL) {
        return -1;
    }

    hal_pwm_impl_t* impl = (hal_pwm_impl_t*)pwm->_impl;
    esp_err_t ret = ledc_set_duty_and_update(impl->speed_mode, impl->channel, duty, 0);
    return (ret == ESP_OK) ? 0 : ret;
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
    free(impl);
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

/* ======================================================================== */
/*  平台驱动层 — 通过 DRIVER_REGISTER + fops 提供标准设备接口                */
/*  上层驱动调 device_ioctl(pwm_dev, PWM_CMD_SET_DUTY, &val) 即可          */
/* ======================================================================== */

#include "driver.h"

typedef struct {
    hal_pwm_channel_t pwm;
} pwm_bl_priv_t;

static int8_t pwm_bl_ioctl(device_t* dev, int cmd, void* arg)
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
        ESP_LOGE(kTag, "pwm_bl: missing pwm_pin");
        return -1;
    }

    pwm_bl_priv_t* priv = (pwm_bl_priv_t*)calloc(1, sizeof(pwm_bl_priv_t));
    if (!priv) return -1;

    hal_pwm_init_struct(&priv->pwm);
    int ret = priv->pwm.init(&priv->pwm, pin, freq, res);
    if (ret != 0) {
        free(priv);
        return ret;
    }

    device_set_priv(dev, priv);
    dev->ops = &pwm_bl_fops;
    ESP_LOGI(kTag, "pwm_bl probed: pin=%d freq=%d res=%d", pin, freq, res);
    return 0;
}

static int pwm_bl_remove(device_t* dev)
{
    pwm_bl_priv_t* priv = (pwm_bl_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->pwm.deinit(&priv->pwm);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(pwm_bl, "esp32,pwm-backlight", pwm_bl_probe, pwm_bl_remove);
