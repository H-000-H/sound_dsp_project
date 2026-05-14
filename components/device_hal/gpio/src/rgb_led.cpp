#include "rgb_led.hpp"

extern "C"
{
#include "bsp_rgb_led.h"
}

struct RgbLight::Impl
{
    rgb_led_handle_t handle = {};
};

 RgbLight::RgbLight() : m_impl(new Impl)
{
}

 RgbLight::~RgbLight()
{
    delete m_impl;
}

 void RgbLight::init(uint32_t gpio_num)
{
    rgb_led_init(&m_impl->handle, gpio_num);
}

 void RgbLight::set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    rgb_led_set_color(&m_impl->handle, red, green, blue);
}

 void RgbLight::set_color(uint32_t color)
{
    rgb_led_set_color_u32(&m_impl->handle, color);
}

 void RgbLight::set_brightness(uint8_t brightness)
{
     rgb_led_set_bright(&m_impl->handle, brightness);
}

 void RgbLight::turn_off()
{
    rgb_led_off(&m_impl->handle);
}

 RgbLight& RgbLight::get_instance()
{
     static RgbLight instance;
    return instance;
}
