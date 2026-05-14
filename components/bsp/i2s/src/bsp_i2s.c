#include "bsp_i2s.h"
#if CONFIG_ENABLE_BSP_I2S
void bsp_i2s_init(bsp_i2s_handle* handle)
{
    ESP_ERROR_CHECK(i2s_new_channel(&handle->channel_config,&handle->tx_channel_handle,NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(handle->tx_channel_handle,&handle->std_config));
    ESP_ERROR_CHECK(i2s_channel_enable(handle->tx_channel_handle));
}

void bsp_i2s_transmit_data(bsp_i2s_handle* handle,int16_t*src,size_t size,size_t* bytes_written,uint32_t time_out)
{
    ESP_ERROR_CHECK(i2s_channel_write(handle->tx_channel_handle,src,size,bytes_written,time_out));
}
#endif