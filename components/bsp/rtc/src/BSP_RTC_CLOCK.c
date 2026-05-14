#include "BSP_RTC_CLOCK.h"
#include "esp_sntp.h"
#if CONFIG_ENABLE_BSP_RTC
struct tm bsp_get_current_time()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

/**
 * @brief 设置RTC时间 非网络版
 */
void bsp_set_rtc_time(BSP_RTC_Clock_HANDLE_t* handle)
{
    if(handle==NULL)return;
    struct tm timeinfo={0};
    if(handle->set_self==false)
    {
        /* 记录当前时间 */
        timeinfo = bsp_get_current_time();
        handle->time_inf.day=timeinfo.tm_mday;
        handle->time_inf.month=timeinfo.tm_mon+1;
        handle->time_inf.year=timeinfo.tm_year+1900;
        handle->time_inf.hour=timeinfo.tm_hour;
        handle->time_inf.minute=timeinfo.tm_min;
        handle->time_inf.second=timeinfo.tm_sec;
    }
    else
    {
        timeinfo.tm_mday =(int) handle->time_inf.day;
        timeinfo.tm_mon = (int)handle->time_inf.month - 1;
        timeinfo.tm_year = (int)handle->time_inf.year - 1900;
        timeinfo.tm_hour = (int)handle->time_inf.hour;
        timeinfo.tm_min = (int)handle->time_inf.minute;
        timeinfo.tm_sec = (int)handle->time_inf.second;
    }

    /* 设置时区 东*/
    setenv("TZ", CONFIG_BSP_RTC_TIMEZONE, 1);
    tzset();

    /*转换为时间戳*/
    time_t t = mktime(&timeinfo);

    /*生成系统调用结构体,给别的系统使用*/
    struct timeval tv = {.tv_sec = t, .tv_usec = 0};

    /*回调回esp32*/
    settimeofday(&tv, NULL);
}

/**
 * @brief 设置RTC时间 网络版
 */
void bsp_set_rtc_time_net(BSP_RTC_Clock_HANDLE_t* handle)
{

    setenv("TZ", CONFIG_BSP_RTC_TIMEZONE, 1);
    tzset();

    esp_netif_sntp_init(&handle->sntp_config);

    /*设置常用ntp时间服务器（阿里云，腾讯云，国家时钟系统）*/
    esp_sntp_setservername(0, CONFIG_BSP_RTC_NTP_SERVER_0);
    esp_sntp_setservername(1, CONFIG_BSP_RTC_NTP_SERVER_1);
    esp_sntp_setservername(2, CONFIG_BSP_RTC_NTP_SERVER_2);
}
#endif
