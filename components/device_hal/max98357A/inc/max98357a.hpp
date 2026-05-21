#pragma once
#include "config.hpp"  
#if CONFIG_USE_MAX_98357A == 1
#include "i2s_bus.hpp"
#include "channel_device.hpp"
#include <cstdint>
#if MAX_98357A_USE_SD 
   constexpr uint32_t MAX_98357A_SD = SD_GPIO_NUM;
#else
    #warning "MAX_98357A_USE_SD is not defined! SD pin should be connected to VCC."
#endif

#if MAX98357A_SINGLE
#define MAX_Channel 1
#elif MAX98357A_Double
#define MAX_Channel 0
#endif
constexpr uint16_t SAMPLE_RATE =         44100;
constexpr uint8_t Channel      =   MAX_Channel;
constexpr uint8_t BitDepth     =            16;
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
    void play_audio(int16_t* src,size_t size,size_t*bytes_written,uint32_t timeout);
};
#endif