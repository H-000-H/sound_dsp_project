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

/* pwm_bl (PWM backlight) 已移至 components/drivers/display/pwm_backlight.c */
