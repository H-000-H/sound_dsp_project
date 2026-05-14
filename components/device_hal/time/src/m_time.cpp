#include "m_time.hpp"

#if CONFIG_ENABLE_DEVICE_HAL_TIME
extern "C"
{
#include "BSP_RTC_CLOCK.h"
}

namespace
{
TIME_INF to_bsp_time(TimeInfo time)
{
    return {
        .year = time.year,
        .month = time.month,
        .day = time.day,
        .hour = time.hour,
        .minute = time.minute,
        .second = time.second,
    };
}

TimeInfo from_bsp_time(TIME_INF time)
{
    return {
        .year = time.year,
        .month = time.month,
        .day = time.day,
        .hour = time.hour,
        .minute = time.minute,
        .second = time.second,
    };
}
}

struct Time::Impl
{
    BSP_RTC_Clock_HANDLE_t time_handler = {};
};

Time::Time() : m_impl(new Impl)
{
}

Time::~Time()
{
    delete m_impl;
}

void Time::init()
{
    if(initialized)
    {
        return;
    }

    set_config();
    apply_default_time();
    initialized = true;
}

void Time::set_config()
{
    auto& time_handler = m_impl->time_handler;
    time_handler.set_self=true;
    time_handler.time_inf=
    {
        .year = DEFAULT_YEAR,
        .month = DEFAULT_MONTH,
        .day = DEFAULT_DAY,
        .hour = DEFAULT_HOUR,
        .minute = DEFAULT_MINUTE,
        .second = DEFAULT_SECOND
    };

    #if CONFIG_ENABLE_TIME_NTP_SYNC
    time_handler.sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    #endif
}

void Time::apply_default_time()
{
    bsp_set_rtc_time(&m_impl->time_handler);
}

void Time::try_start_ntp_sync()
{
    #if CONFIG_ENABLE_TIME_NTP_SYNC
    if(ntp_started)
    {
        return;
    }

    if(m_net_checker && !m_net_checker())
    {
        return;
    }

    bsp_set_rtc_time_net(&m_impl->time_handler);
    ntp_started = true;
    #endif
}

void Time::sync_ntp()
{
    try_start_ntp_sync();
}

void Time::set_net_available_checker(NetAvailableCheck check)
{
    m_net_checker = std::move(check);
}

#if CONFIG_ENABLE_TIME_MANUAL_SET

void Time::set_year(uint16_t year)
{
    m_impl->time_handler.time_inf.year = year;
    bsp_set_rtc_time(&m_impl->time_handler);
}

void Time::set_month(uint8_t month)
{
    m_impl->time_handler.time_inf.month = month;
    bsp_set_rtc_time(&m_impl->time_handler);
}

void Time::set_day(uint8_t day)
{
    m_impl->time_handler.time_inf.day = day;
    bsp_set_rtc_time(&m_impl->time_handler);
}

void Time::set_hour(uint8_t hour)
{
    m_impl->time_handler.time_inf.hour = hour;
    bsp_set_rtc_time(&m_impl->time_handler);
}
    
void Time::set_minute(uint8_t minute)
{
    m_impl->time_handler.time_inf.minute = minute;
    bsp_set_rtc_time(&m_impl->time_handler);
}
    
    
void Time::set_second(uint8_t second)
{
    m_impl->time_handler.time_inf.second = second;
    bsp_set_rtc_time(&m_impl->time_handler);
}

void Time::set_time(TimeInfo time_inf)
{
    m_impl->time_handler.time_inf = to_bsp_time(time_inf);
    bsp_set_rtc_time(&m_impl->time_handler);
}

#endif

TimeInfo Time::get_time()
{
    if(!initialized)
    {
        init();
    }

    struct tm now = bsp_get_current_time();
    auto& time_inf = m_impl->time_handler.time_inf;
    time_inf.year = (uint16_t)(now.tm_year + 1900);
    time_inf.month = (uint8_t)(now.tm_mon + 1);
    time_inf.day = (uint8_t)now.tm_mday;
    time_inf.hour = (uint8_t)now.tm_hour;
    time_inf.minute = (uint8_t)now.tm_min;
    time_inf.second = (uint8_t)now.tm_sec;
    return from_bsp_time(time_inf);
}

#endif
