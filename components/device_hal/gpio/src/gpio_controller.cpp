#include "gpio_controller.hpp"

extern "C"
{
#include "bsp_gpio.h"
}

namespace
{
gpio_mode_t to_bsp_mode(GpioMode mode)
{
    switch (mode)
    {
        case GpioMode::Input: return GPIO_MODE_INPUT;
        case GpioMode::Output: return GPIO_MODE_OUTPUT;
        case GpioMode::InputOutput: return GPIO_MODE_INPUT_OUTPUT;
        default: return GPIO_MODE_DISABLE;
    }
}

gpio_pullup_t to_bsp_pullup(GpioPull pull)
{
    return pull == GpioPull::Enable ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
}

gpio_pulldown_t to_bsp_pulldown(GpioPull pull)
{
    return pull == GpioPull::Enable ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
}

gpio_int_type_t to_bsp_interrupt(GpioInterrupt intr)
{
    switch (intr)
    {
        case GpioInterrupt::RisingEdge: return GPIO_INTR_POSEDGE;
        case GpioInterrupt::FallingEdge: return GPIO_INTR_NEGEDGE;
        case GpioInterrupt::AnyEdge: return GPIO_INTR_ANYEDGE;
        case GpioInterrupt::Disable:
        default:
            return GPIO_INTR_DISABLE;
    }
}
}

GpioController::GpioController() = default;

GpioController::~GpioController() = default;

void GpioController::init(uint32_t gpio_num, GpioMode mode,
                          GpioPull pull_up_en,
                          GpioPull pull_down_en,
                          GpioInterrupt intr_type)
{
    gpio_driver_init(gpio_num,
                     to_bsp_mode(mode),
                     to_bsp_pullup(pull_up_en),
                     to_bsp_pulldown(pull_down_en),
                     to_bsp_interrupt(intr_type));
}

int GpioController::get_level(uint32_t gpio_num)
{
    return gpio_driver_get_level(gpio_num);
}

void GpioController::set_level(uint32_t gpio_num, uint32_t level)
{
    gpio_driver_set_level(gpio_num, level);
}

void GpioController::toggle_level(uint32_t gpio_num)
{
    gpio_driver_toggle_level(gpio_num);
}

GpioController& GpioController::get_instance()
{
    static GpioController instance;
    return instance;
}

void GpioController::install_isr_service()
{
    gpio_driver_install_isr_service();
}

void GpioController::add_isr_handler(uint32_t gpio_num, GpioIsrHandler handler, void* args)
{
    gpio_driver_add_isr_handler(gpio_num, reinterpret_cast<gpio_isr_t>(handler), args);
}

 SingleColorLed::SingleColorLed() = default;

 SingleColorLed::~SingleColorLed() = default;

 void SingleColorLed::init(uint32_t gpio_num)
{
     m_gpio_num = gpio_num;
     m_level = LED_OFF;
    led_driver_init(gpio_num);
}

 void SingleColorLed::toggle()
{
     m_level = !m_level;
     gpio_driver_set_level(m_gpio_num, m_level);
}

 void SingleColorLed::turn_on()
{
     m_level = LED_ON;
     gpio_driver_set_level(m_gpio_num, LED_ON);
}

 void SingleColorLed::turn_off()
{
     m_level = LED_OFF;
     gpio_driver_set_level(m_gpio_num, LED_OFF);
}

 SingleColorLed& SingleColorLed::get_instance()
{
     static SingleColorLed instance;
    return instance;
}

 uint8_t SingleColorLed::level() const
{
     return m_level;
}
