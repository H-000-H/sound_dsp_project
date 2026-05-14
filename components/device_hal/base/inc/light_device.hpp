#pragma once

#include <cstdint>

class LightDevice
{
public:
    virtual void init(uint32_t gpio_num) = 0;
    virtual void turn_off() = 0;

protected:
    virtual ~LightDevice() = default;
};

class BinaryLightDevice : public LightDevice
{
public:
    virtual void turn_on() = 0;
    virtual void toggle() = 0;

protected:
    ~BinaryLightDevice() override = default;
};

class ColorLightDevice : public LightDevice
{
public:
    virtual void set_color(uint32_t color) = 0;
    virtual void set_brightness(uint8_t brightness) = 0;

protected:
    ~ColorLightDevice() override = default;
};
