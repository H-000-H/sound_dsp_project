#pragma once

#include "light_device.hpp"

#include <cstdint>

constexpr uint8_t LED_ON = 1;
constexpr uint8_t LED_OFF = 0;

enum class GpioMode
{
    Input,
    Output,
    InputOutput,
};

enum class GpioPull
{
    Disable,
    Enable,
};

enum class GpioInterrupt
{
    Disable,
    RisingEdge,
    FallingEdge,
    AnyEdge,
};

using GpioIsrHandler = void (*)(void* args);

class GpioController
{
protected:
    GpioController();
    ~GpioController();

public:
    void init(uint32_t gpio_num, GpioMode mode,
              GpioPull pull_up_en = GpioPull::Disable,
              GpioPull pull_down_en = GpioPull::Disable,
              GpioInterrupt intr_type = GpioInterrupt::Disable);

    int get_level(uint32_t gpio_num);

    void set_level(uint32_t gpio_num, uint32_t level);

    void toggle_level(uint32_t gpio_num);

    static GpioController& get_instance();

    static void install_isr_service();
    static void add_isr_handler(uint32_t gpio_num, GpioIsrHandler handler, void* args);
};

class SingleColorLed : public BinaryLightDevice
{
private:
    SingleColorLed();
    ~SingleColorLed();
    uint32_t m_gpio_num = 0;
    uint8_t m_level = LED_OFF;

public:
    void init(uint32_t gpio_num) override;
    void toggle() override;
    void turn_on() override;
    void turn_off() override;
    static SingleColorLed& get_instance();
    uint8_t level() const;
};
