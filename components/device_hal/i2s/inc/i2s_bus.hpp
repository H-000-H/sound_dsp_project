#pragma once
#include "channel_device.hpp"
#include <cstddef>
#include <cstdint>
typedef int16_t I2sSample;

constexpr uint32_t WS_GPIO_NUM = 13;
constexpr uint32_t BCLK_GPIO_NUM = 12;
constexpr uint32_t DOUT_GPIO_NUM = 11;
constexpr uint32_t SD_GPIO_NUM = 10;
constexpr uint32_t I2S_BUS_SAMPLE_RATE = 44100;

class I2sAudioBus : public ChannelDevice
{
private:
    I2sAudioBus();
    ~I2sAudioBus();
    struct Impl;
    Impl* m_impl;
    void set_config() override;
public:
    static I2sAudioBus& get_instance();
    void init() override;
    /**
     * @brief 发送 I2S 采样数据
     * @param src 采样数据缓冲区
     * @param size 缓冲区大小
     * @param bytes_written 实际发送字节数，可为空
     * @param time_out 超时时间，单位为毫秒
     */
    void write_samples(I2sSample* src, size_t size, size_t* bytes_written, uint32_t time_out);
};
