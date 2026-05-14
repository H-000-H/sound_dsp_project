#pragma once
#include "light_device.hpp"
#include <cstdint>

constexpr uint32_t red=0xFF0000;
constexpr uint32_t green=0x00FF00;
constexpr uint32_t blue=0x0000FF;
constexpr uint32_t white=0xFFFFFF;
constexpr uint32_t black=0x000000;
constexpr uint32_t yellow=0xFFFF00;
constexpr uint32_t cyan=0x00FFFF;   // 青色
constexpr uint32_t magenta=0xFF00FF; // 品红色
constexpr uint32_t pink=0xFF69B4;

class RgbLight : public ColorLightDevice
{
private:
    RgbLight();
    ~RgbLight();
    struct Impl;
    Impl* m_impl;
public:
    void init(uint32_t gpio_num) override;
    void set_color(uint8_t red,uint8_t green,uint8_t blue);
    void set_color(uint32_t color) override;
    void set_brightness(uint8_t brightness) override;
    void turn_off() override;
    static RgbLight& get_instance();
};
