#pragma once
#include "i2s_bus.hpp"
#include "channel_device.hpp"
#include <cstdint>
#ifndef MAX_98357A_USE_SD       
#define MAX_98357A_USE_SD 1
#endif
#if MAX_98357A_USE_SD 
   constexpr uint32_t MAX_98357A_SD = 9;
#endif
class MAX_98357A : public ChannelDevice
{
private:
    MAX_98357A() = default;
    ~MAX_98357A() = default;
    void set_config() override;
public:
    void init() override;    
    static MAX_98357A& get_instance(){static MAX_98357A instance;return instance;};
    void start_slince();
    void start_Play();
};
