#include "pwm_controller.hpp"

#include <cstring>

extern "C"
{
#include "bsp_pwm.h"
}

struct PwmOutputChannel::Impl
{
    bsp_pwm_handle_t handler = {};
};

 PwmOutputChannel::PwmOutputChannel() : m_impl(new Impl)
{
}

 PwmOutputChannel::~PwmOutputChannel()
{
    delete m_impl;
}

 PwmOutputChannel& PwmOutputChannel::get_instance()
{
     static PwmOutputChannel instance;
    return instance;
}

 void PwmOutputChannel::init()
{
    set_config();
    bsp_pwm_init(&m_impl->handler);
}

 void PwmOutputChannel::set_config()
{
    auto& handler = m_impl->handler;
    memset(&handler, 0, sizeof(handler));
    handler.channel_config.channel = static_cast<ledc_channel_t>(LCD_BACKLIGHT_PWM_CHANNEL);
    handler.channel_config.gpio_num = CONFIG_BSP_LCD_ST7789_PIN_BLK;
    handler.channel_config.flags.output_invert = PWM_OUTPUT_INVERT_DISABLE;
    handler.channel_config.hpoint = 0;
    handler.channel_config.intr_type = LEDC_INTR_DISABLE;
    handler.channel_config.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    handler.channel_config.speed_mode = LEDC_LOW_SPEED_MODE;
    handler.channel_config.timer_sel = LEDC_TIMER_0;

    handler.duty = 255;

    handler.speed_mode = LEDC_LOW_SPEED_MODE;
    handler.channel = static_cast<ledc_channel_t>(LCD_BACKLIGHT_PWM_CHANNEL);

    handler.tim_config.clk_cfg = LEDC_AUTO_CLK;
    handler.tim_config.duty_resolution = LEDC_TIMER_8_BIT;
    handler.tim_config.freq_hz = 5000;
    handler.tim_config.speed_mode = LEDC_LOW_SPEED_MODE;
    handler.tim_config.timer_num = LEDC_TIMER_0;
}

void PwmOutputChannel::set_duty(uint32_t duty)
{
    bsp_pwm_set_duty(duty, &m_impl->handler);
}
