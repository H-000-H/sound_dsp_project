/*本文件有两个RTC实现,一个是没有联网的RTC,一个是联网的RTC ESP32的实时时钟是和PC同步的所以直接引用time库*/
/*因为本文件不算外设驱动而且包含网络使用而且是由app层的lvgl使用
所以不放在hal文件夹下,本人打算直接放在server层单独一个文件夹下
为保证上层app使用一样的函数和参数本人会去factory里面配置统一的time工厂提供app使用
但此工厂配置细节为适配不同芯片而定因此和外设工厂会有细微区别*/
#ifndef BSP_RTC_CLOCK_H
#define BSP_RTC_CLOCK_H
#ifdef __cplusplus
extern "C" 
{
#endif
#include "config.hpp"
#if CONFIG_ENABLE_BSP_RTC
#include <time.h>
#include <sys/time.h>
#include "esp_netif_sntp.h"
typedef struct 
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
}TIME_INF;

typedef struct 
{
    esp_sntp_config_t sntp_config;
    TIME_INF time_inf;
    bool set_self;
} BSP_RTC_Clock_HANDLE_t;
/**
 * @brief 获取当前时间
 */
struct tm bsp_get_current_time();

/**
 * @brief 设置RTC时间,非联网版
 */
void bsp_set_rtc_time(BSP_RTC_Clock_HANDLE_t* param);

/**
 * @brief 设置RTC时间,联网版
 */
void bsp_set_rtc_time_net(BSP_RTC_Clock_HANDLE_t* param);

#ifdef __cplusplus
}
#endif
#endif
#endif // BSP_RTC_CLOCK_H
