#pragma once
#include "config.hpp"
#include "channel_device.hpp"
#include <cstdint>
#include <functional>
#if CONFIG_ENABLE_DEVICE_HAL_TIME
constexpr uint16_t DEFAULT_YEAR=2026;
constexpr uint8_t DEFAULT_MONTH=1;
constexpr uint8_t DEFAULT_DAY=1;
constexpr uint8_t DEFAULT_HOUR=0;
constexpr uint8_t DEFAULT_MINUTE=0;
constexpr uint8_t DEFAULT_SECOND=0;

struct TimeInfo
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

class Time :public ChannelDevice
{
public:
    static Time& get_instance(){static Time instance;return instance;};
    void init() override;
    void set_config() override;
    using NetAvailableCheck = std::function<bool()>;
    void set_net_available_checker(NetAvailableCheck check);
    void sync_ntp();
    #if CONFIG_ENABLE_TIME_MANUAL_SET
    void set_year(uint16_t year);
    void set_month(uint8_t month);
    void set_day(uint8_t day);
    void set_hour(uint8_t hour);
    void set_minute(uint8_t minute);
    void set_second(uint8_t second);
    void set_time(TimeInfo time);
    #endif
    TimeInfo get_time();
private:
    Time();
    ~Time();
    void apply_default_time();
    void try_start_ntp_sync();
    struct Impl;
    Impl* m_impl;
    NetAvailableCheck m_net_checker;
    bool initialized = false;
    bool ntp_started = false;
};
#endif
