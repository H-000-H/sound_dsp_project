#include "i2s_bus.hpp"

extern "C"
{
#include "bsp_i2s.h"
}

struct I2sAudioBus::Impl
{
    bsp_i2s_handle handler = {};
};

 I2sAudioBus::I2sAudioBus() : m_impl(new Impl)
{
}

 I2sAudioBus::~I2sAudioBus()
{
    delete m_impl;
}

 I2sAudioBus& I2sAudioBus::get_instance()
{
     static I2sAudioBus instance;
    return instance;
}

void I2sAudioBus::set_config()
{
    auto& handler = m_impl->handler;
    handler.channel_config=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO,I2S_ROLE_MASTER);
    /*采样率双声道标准模式*/
    handler.std_config.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(I2S_BUS_SAMPLE_RATE);
    handler.std_config.slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
    handler.std_config.gpio_cfg.bclk=(gpio_num_t)BCLK_GPIO_NUM;
    handler.std_config.gpio_cfg.dout=(gpio_num_t)DOUT_GPIO_NUM;
    handler.std_config.gpio_cfg.ws=(gpio_num_t)WS_GPIO_NUM;
    handler.std_config.gpio_cfg.din=I2S_GPIO_UNUSED;

    /*不开启翻转*/
    handler.std_config.gpio_cfg.invert_flags.bclk_inv=false;
    handler.std_config.gpio_cfg.invert_flags.mclk_inv=false;
    handler.std_config.gpio_cfg.invert_flags.ws_inv=false;
} 

void I2sAudioBus::init()
{
    set_config();
    bsp_i2s_init(&m_impl->handler);
}

void I2sAudioBus::write_samples(I2sSample* src, size_t size, size_t* bytes_written, uint32_t time_out)
{
    bsp_i2s_transmit_data(&m_impl->handler, src, size, bytes_written, time_out);
}
