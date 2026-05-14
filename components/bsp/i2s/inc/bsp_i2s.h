#ifndef __BSP_I2S_H__
#define __BSP_I2S_H__
#include "config.hpp"
#if CONFIG_ENABLE_BSP_I2S
#ifdef __cplusplus
extern "C"
{
#endif
/*查看i2s_std和i2s_common文件夹的函数*/
#include <driver/i2s_std.h>
#include <stdint.h>
typedef struct
{
    i2s_std_config_t std_config;
    /*这两只用什么功能初始化什么功能不需要全初始化创建channel时可以直接传null就行*/
    i2s_chan_handle_t tx_channel_handle;
    i2s_chan_handle_t rx_channel_handle;
    i2s_chan_config_t channel_config;
}bsp_i2s_handle;

void bsp_i2s_init(bsp_i2s_handle* param);

/**
 * @brief 发送i2s数据
 * @param param i2s句柄
 * @param src 数据指针(该参数传递音频保存的指针)
 * @param size 数据大小
 * @param bytes_written 实际发送字节数指针(可选)
 * @param time_out 超时时间
 */
void bsp_i2s_transmit_data(bsp_i2s_handle* param,int16_t*src,size_t size,size_t* bytes_written,uint32_t time_out);
#ifdef __cplusplus
}
#endif
#endif
#endif
