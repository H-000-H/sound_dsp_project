#pragma once
#include "channel_device.hpp"
#include <cstdint>
constexpr uint8_t PWM_OUTPUT_INVERT_DISABLE = 0;
constexpr uint8_t LCD_BACKLIGHT_PWM_CHANNEL = 0;

class PwmOutputChannel : public ChannelDevice
{
private:
    PwmOutputChannel();
    ~PwmOutputChannel();
    struct Impl;
    Impl* m_impl;
    void set_config() override;
public:
    static PwmOutputChannel& get_instance();
    void init() override;
    void set_duty(uint32_t duty);
};
