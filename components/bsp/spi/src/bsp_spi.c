#include "bsp_spi.h"
#include "esp_log.h"
#if CONFIG_ENABLE_BSP_SPI

void bsp_spi_init(bsp_spi_handle* handler)
{
    esp_err_t ret;
    ret = spi_bus_initialize((handler->SPIx),&(handler->bus_cfg),(handler->dma_mode));
    if(ret != ESP_OK)
    {
        ESP_LOGE("SpiBus", "spi_bus_initialize failed: %d", ret);
    }
    ret = spi_bus_add_device((handler->SPIx),&(handler->interface_cfg),(&handler->spi_dev));
    if(ret != ESP_OK)
    {
        ESP_LOGE("SpiBus", "spi_bus_add_device failed: %d", ret);
    }
}
void bsp_spi_send(bsp_spi_handle* handle,const uint8_t*data,uint32_t len)
{
    if(handle->spi_dev == NULL)
    {
        ESP_LOGE("SpiBus", "spi_dev is NULL!");
        return;
    }
    // 如需调试发送内容，可临时打开这条日志。
    spi_transaction_t trans={0};
    trans.length = (size_t)len * 8; // SPI 事务长度单位是位
    trans.tx_buffer = data;
    esp_err_t ret = spi_device_transmit(handle->spi_dev,&trans);
    if(ret != ESP_OK)
    {
        ESP_LOGE("SpiBus", "spi_device_transmit failed: %d", ret);
    }
}
void bsp_spi_recv(bsp_spi_handle* handle, uint8_t* buffer, uint32_t len)
{
    spi_transaction_t trans = {0};
    trans.length = (size_t)len * 8;  // SPI 接收长度单位同样是位
    trans.rxlength = (size_t)len * 8;
    trans.rx_buffer = buffer;
    spi_device_transmit(handle->spi_dev, &trans);
}
#endif
