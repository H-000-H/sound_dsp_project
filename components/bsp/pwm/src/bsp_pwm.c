#include "bsp_pwm.h"
#if CONFIG_ENABLE_BSP_PWM
static bool s_ledc_fade_installed = false;

void bsp_timer_init(bsp_timer_handle_t* handler)
{
    ESP_ERROR_CHECK(timer_init(handler->timer_group,handler->timer_idx,&handler->timer_config));
    ESP_ERROR_CHECK(timer_start(handler->timer_group,handler->timer_idx));
    ESP_ERROR_CHECK(timer_set_counter_value(handler->timer_group,handler->timer_idx, handler->default_start_value));
}

void bsp_pwm_init(bsp_pwm_handle_t* handle)
{
    if (!s_ledc_fade_installed)
    {
        esp_err_t fade_ret = ledc_fade_func_install(0);
        if (fade_ret != ESP_OK && fade_ret != ESP_ERR_INVALID_STATE)
        {
            ESP_ERROR_CHECK(fade_ret);
        }
        s_ledc_fade_installed = true;
    }
    ESP_ERROR_CHECK(ledc_timer_config(&handle->tim_config));
    ESP_ERROR_CHECK(ledc_channel_config(&handle->channel_config));
    /* 使用线程安全接口，避免占空比更新过程出现不一致。 */
    ESP_ERROR_CHECK(ledc_set_duty_and_update(
        handle->channel_config.speed_mode,
        handle->channel_config.channel,
        handle->duty,
        handle->channel_config.hpoint));
}   

void bsp_pwm_set_duty(uint32_t duty, bsp_pwm_handle_t* handle)
{
    ESP_ERROR_CHECK(ledc_set_duty_and_update(handle->speed_mode,
                                             handle->channel,
                                             duty,
                                             handle->channel_config.hpoint));
}

#endif

