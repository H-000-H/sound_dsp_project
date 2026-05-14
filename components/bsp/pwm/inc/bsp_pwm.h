#ifndef __BSP_PWM_H__
#define __BSP_PWM_H__
#include "config.hpp"
#if CONFIG_ENABLE_BSP_PWM
#ifdef __cplusplus
extern "C"
{
#endif
/* ESP32 的 PWM 使用 LEDC 外设，这里仍保留通用定时器配置结构。 */

#include <driver/timer.h>
#include <driver/ledc.h>
typedef struct 
{
    timer_group_t timer_group;
    timer_idx_t timer_idx;
    timer_config_t timer_config;
    timer_count_dir_t count_dir;
    uint32_t default_start_value;
} bsp_timer_handle_t;
void bsp_timer_init(bsp_timer_handle_t* param);
typedef struct
{
    ledc_channel_config_t channel_config;
    ledc_timer_config_t   tim_config;
    ledc_mode_t           speed_mode;
    ledc_channel_t        channel;
    uint32_t              duty;
} bsp_pwm_handle_t;

void bsp_pwm_init(bsp_pwm_handle_t* param);
void bsp_pwm_set_duty(uint32_t duty, bsp_pwm_handle_t* param);
#ifdef __cplusplus
}
#endif
#endif
#endif
