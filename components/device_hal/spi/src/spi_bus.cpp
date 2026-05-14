#include "spi_bus.hpp"

#include <cstring>
#include "driver/gpio.h"

extern "C"
{
#include "bsp_spi.h"
}

struct SpiMasterBus::Impl
{
    bsp_spi_handle handler = {};
};

 SpiMasterBus::SpiMasterBus() : m_impl(new Impl)
{
}

 SpiMasterBus::~SpiMasterBus()
{
    delete m_impl;
}

 SpiMasterBus& SpiMasterBus::get_instance()
{
     static SpiMasterBus instance;
    return instance;
}

 void SpiMasterBus::init()
{
    bsp_spi_init(&m_impl->handler);
}

 void SpiMasterBus::write_block(const uint8_t* data, size_t size)
 {
     if (data == nullptr || size == 0)
     {
         return;
     }
     bsp_spi_send(&m_impl->handler, data, size);
 }
 
 void SpiMasterBus::read_block(uint8_t* buffer, size_t size)
{
     if (buffer == nullptr || size == 0)
     {
         return;
     }
     bsp_spi_recv(&m_impl->handler, buffer, size);
}

 void SpiMasterBus::set_config()
{
    auto& handler = m_impl->handler;
    memset(&(handler.bus_cfg), 0, sizeof(spi_bus_config_t));
    handler.bus_cfg.mosi_io_num = CONFIG_BSP_LCD_ST7789_PIN_MOSI;
    handler.bus_cfg.miso_io_num = -1;
    handler.bus_cfg.sclk_io_num = CONFIG_BSP_LCD_ST7789_PIN_CLK;
    handler.bus_cfg.quadwp_io_num = -1;
    handler.bus_cfg.quadhd_io_num = -1;
    handler.bus_cfg.max_transfer_sz = 32768;

    handler.dma_mode = SPI_DMA_CH_AUTO;

    memset(&(handler.interface_cfg), 0, sizeof(spi_device_interface_config_t));
    handler.interface_cfg.mode = 3;
    handler.interface_cfg.clock_speed_hz = SPI_BUS_80MHZ;
    handler.interface_cfg.spics_io_num = -1;
    handler.interface_cfg.flags = SPI_DEVICE_HALFDUPLEX;
    handler.interface_cfg.queue_size = 7;

    handler.SPIx = SPI2_HOST;
}

 void* SpiMasterBus::native_handle()
{
    return &m_impl->handler;
}
