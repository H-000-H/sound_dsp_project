#include "max98357A.hpp"

#include "gpio_controller.hpp"

void MAX_98357A::init()
{
#if MAX_98357A_USE_SD
    auto& gpio = GpioController::get_instance();
    gpio.init(MAX_98357A_SD, GpioMode::Output);
    gpio.set_level(MAX_98357A_SD, 1);
#endif
}

void MAX_98357A::set_config()
{
    
}

void MAX_98357A::start_slince()
{
    GpioController::get_instance().set_level(MAX_98357A_SD, 0);
}

void MAX_98357A::start_Play()
{
    GpioController::get_instance().set_level(MAX_98357A_SD, 1);
}
