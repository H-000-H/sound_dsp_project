#ifndef BSP_SPI_H
#define BSP_SPI_H
#include "config.hpp"
#if CONFIG_ENABLE_BSP_SPI
#ifdef __cplusplus
extern "C"
{
#endif
#include <string.h>
#include "driver/spi_master.h"
/**
 * @brief 初始化 SPI
 * @details 当前使用 3 线 SPI 模式，GPIO11 为 MOSI，GPIO12 为 SPI 时钟
 * @details 本次 SPI 用于连接屏幕，因此默认配置为半双工模式
 */
typedef struct
{
    spi_bus_config_t bus_cfg;
    spi_device_interface_config_t interface_cfg;
    spi_host_device_t SPIx;
    spi_dma_chan_t dma_mode;
    spi_device_handle_t spi_dev; // 保存设备句柄
}bsp_spi_handle;
void bsp_spi_init(bsp_spi_handle* param);
void bsp_spi_send(bsp_spi_handle* param,const uint8_t*data,uint32_t len);
void bsp_spi_recv(bsp_spi_handle* param,uint8_t*buffer,uint32_t len);
#ifdef __cplusplus
}

#endif
#endif
#endif
