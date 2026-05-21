#include "max98357a.hpp"
#if CONFIG_USE_MAX_98357A == 1

#include "gpio_controller.hpp"

void MAX_98357A::init()
{
    auto &i2s = I2sAudioBus::get_instance();
    i2s.init();
#if MAX_98357A_USE_SD == 1 
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

void MAX_98357A::play_audio(int16_t* src,size_t size,size_t*bytes_written,uint32_t timeout)
{
    auto& i2s =I2sAudioBus::get_instance();
    i2s.write_samples(src,size,bytes_written,timeout);
}
#endif