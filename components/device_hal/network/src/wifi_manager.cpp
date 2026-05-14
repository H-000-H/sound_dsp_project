#include "wifi_manager.hpp"
#if CONFIG_ENABLE_DEVICE_HAL_WIFI

extern "C"
{
#include "bsp_wifi.h"
}

#include <cstring>

namespace
{
wifi_mode_t to_bsp_wifi_mode(WifiMode mode)
{
    switch (mode)
    {
        case WifiMode::Sta: return WIFI_MODE_STA;
        case WifiMode::Ap: return WIFI_MODE_AP;
        case WifiMode::ApSta: return WIFI_MODE_APSTA;
        default: return WIFI_MODE_STA;
    }
}
}

struct WifiManager::Impl
{
    bsp_wifi_handle_t handle = {};
    wifi_disconnected_cb_t disconnected_cb = nullptr;

    static void on_disconnected_wrapper(void)
    {
        WifiManager::get_instance().on_disconnected();
    }
};

void WifiManager::on_disconnected()
{
    if (m_impl->disconnected_cb) {
        m_impl->disconnected_cb();
    }
}

WifiManager::WifiManager() : m_impl(new Impl)
{
}

WifiManager::~WifiManager()
{
    delete m_impl;
}

WifiManager& WifiManager::get_instance()
{
    static WifiManager instance;
    return instance;
}

void WifiManager::init()
{
    bsp_wifi_init(&m_impl->handle);
    bsp_wifi_set_disconnected_cb(&m_impl->handle, Impl::on_disconnected_wrapper);
}

void WifiManager::connect()
{
    bsp_wifi_connect(&m_impl->handle);
}

void WifiManager::stop()
{
    bsp_wifi_stop(&m_impl->handle);
}

void WifiManager::disconnect()
{
    bsp_wifi_disconnect(&m_impl->handle);
}

bool WifiManager::is_connected() const
{
    return bsp_wifi_is_connected(&m_impl->handle);
}

void WifiManager::set_disconnected_cb(wifi_disconnected_cb_t cb)
{
    m_impl->disconnected_cb = cb;
}

void WifiManager::set_ssid(const uint8_t* ssid)
{
    if (ssid)
    {
        memcpy(m_impl->handle.cfg.ssid, ssid, sizeof(m_impl->handle.cfg.ssid));
    }
}

void WifiManager::set_sta_password(const uint8_t* password)
{
    if (password)
    {
        memcpy(m_impl->handle.cfg.sta_password, password, sizeof(m_impl->handle.cfg.sta_password));
    }
}

void WifiManager::set_ap_name(const uint8_t* name)
{
    if (name)
    {
        memcpy(m_impl->handle.cfg.ap_name, name, sizeof(m_impl->handle.cfg.ap_name));
    }
}

void WifiManager::set_channel(uint8_t channel)
{
    m_impl->handle.cfg.channel = channel;
}

void WifiManager::set_ap_password(const uint8_t* password)
{
    if (password)
    {
        memcpy(m_impl->handle.cfg.ap_password, password, sizeof(m_impl->handle.cfg.ap_password));
    }
}

void WifiManager::set_ip(uint32_t ip)
{
    m_impl->handle.cfg.ip = ip;
}

void WifiManager::set_mask(uint32_t mask)
{
    m_impl->handle.cfg.mask = mask;
}

void WifiManager::set_gateway(uint32_t gateway)
{
    m_impl->handle.cfg.gateway = gateway;
}

void WifiManager::set_dns(uint32_t dns)
{
    m_impl->handle.cfg.dns = dns;
}

void WifiManager::set_wifi_mode(WifiMode mode)
{
    m_impl->handle.cfg.wifi_mode = to_bsp_wifi_mode(mode);
}

void WifiManager::set_max_connection_count(uint8_t count)
{
    m_impl->handle.cfg.max_connection_count = count;
}
#endif
