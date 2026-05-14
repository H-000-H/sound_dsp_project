#ifndef __BSP_WIFI_H__
#define __BSP_WIFI_H__
#include "config.hpp"
#if CONFIG_ENABLE_BSP_WIFI
#include <stdint.h>
#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#ifdef __cplusplus
extern "C"
{
#endif

/** WiFi 意外断连回调 — 由 BSP 在非手动断连时调用，用于通知应用层更新 UI 等 */
typedef void (*bsp_wifi_disconnected_cb_t)(void);

typedef struct 
{
    uint8_t ssid[32];
    uint8_t sta_password[64];
    uint8_t ap_name[32];
    uint8_t channel;
    uint8_t ap_password[64];
    uint32_t ip;
    uint32_t mask;
    uint32_t gateway;
    uint32_t dns;
    wifi_mode_t wifi_mode;
    uint8_t max_connection_count;
} bsp_wifi_config_t;

typedef struct
{
    bsp_wifi_config_t cfg;
    bool is_connected;
    bool manual_disconnect;
    /** WiFi 意外断连回调（非手动断开时触发）*/
    bsp_wifi_disconnected_cb_t on_disconnected;
} bsp_wifi_handle_t;

void bsp_wifi_init(bsp_wifi_handle_t* handle);
void bsp_wifi_connect(bsp_wifi_handle_t* handle);
void bsp_wifi_disconnect(bsp_wifi_handle_t* handle);
void bsp_wifi_stop(bsp_wifi_handle_t* handle);
bool bsp_wifi_is_connected(bsp_wifi_handle_t* handle);

/** 注册 WiFi 意外断连回调 */
void bsp_wifi_set_disconnected_cb(bsp_wifi_handle_t* handle, bsp_wifi_disconnected_cb_t cb);

#ifdef __cplusplus
}
#endif
#endif
#endif
