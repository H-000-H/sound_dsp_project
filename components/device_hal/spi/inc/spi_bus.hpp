#pragma once
#include "channel_device.hpp"
#include <cstddef>
#include <cstdint>
constexpr uint32_t SPI_BUS_80MHZ = 80 * 1000 * 1000;
class SpiMasterBus : public ChannelDevice
{
private:
    SpiMasterBus();
    ~SpiMasterBus();
    void set_config() override;
    struct Impl;
    Impl* m_impl;
public:
    static SpiMasterBus& get_instance();
    void init() override;
    void write_block(const uint8_t* data, size_t size);
    void read_block(uint8_t* buffer, size_t size);
    void get_config(){set_config();}
    void* native_handle();
};
