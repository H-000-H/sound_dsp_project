#pragma once

#include "config.hpp"
#include <cstdint>
#include <functional>

#if CONFIG_ENABLE_DEVICE_HAL_WIFI
enum class WifiMode
{
    Sta,
    Ap,
    ApSta,
};

/** WiFi 意外断连回调 */
using wifi_disconnected_cb_t = std::function<void()>;

class WifiManager
{
public:
    static WifiManager& get_instance();

    void init();
    void connect();
    /** 完全停止 WiFi 驱动 */
    void stop();
    void disconnect();
    bool is_connected() const;

    void set_ssid(const uint8_t* ssid);
    void set_sta_password(const uint8_t* password);
    void set_ap_name(const uint8_t* name);
    void set_channel(uint8_t channel);
    void set_ap_password(const uint8_t* password);
    void set_ip(uint32_t ip);
    void set_mask(uint32_t mask);
    void set_gateway(uint32_t gateway);
    void set_dns(uint32_t dns);
    void set_wifi_mode(WifiMode mode);
    void set_max_connection_count(uint8_t count);

    /** 注册 WiFi 意外断连回调 */
    void set_disconnected_cb(wifi_disconnected_cb_t cb);

private:
    WifiManager();
    ~WifiManager();
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    /** BSP 断连回调转发（由静态包装器调用）*/
    void on_disconnected();

    struct Impl;
    Impl* m_impl;
};
#endif
