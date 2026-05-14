#pragma once
#include "config.hpp"
#include "channel_device.hpp"
#include <cstdint>
#if CONFIG_ENABLE_DEVICE_HAL_BLUE
class Blue:public ChannelDevice
{
private:
    Blue();
    ~Blue();
    struct Impl;
    Impl* m_impl;
    void set_config() override;
public:
    void init() override;
    static Blue& get_instance(){static Blue instance;return instance;};
    void connect(const uint8_t remote_bda[6]);
};
#endif
