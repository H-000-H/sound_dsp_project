#ifndef __BSP_BLUE_H__
#define __BSP_BLUE_H__
#ifdef __cplusplus
extern "C" {
#endif  
#include "config.hpp"
#include "soc/soc_caps.h"
#if CONFIG_ENABLE_BSP_BLUE
#if defined(SOC_BT_SUPPORTED) && SOC_BT_SUPPORTED && defined(SOC_BT_CLASSIC_SUPPORTED) && SOC_BT_CLASSIC_SUPPORTED
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_err.h"
typedef struct
{
    esp_bt_controller_config_t bt_cfg;
    esp_bt_mode_t  MODE[2];//0:ble,1:classic 
    const char* bluebooth_name;
    esp_a2d_cb_t a2d_cb;//a2d回调函数
    esp_a2d_source_data_cb_t a2d_source_data_cb;//a2d source data回调函数
    esp_bt_gap_cb_t gap_cb;//gap回调函数
    esp_bt_connection_mode_t c_mode;//连接模式
    esp_bt_discovery_mode_t d_mode;//发现模式
    esp_bt_inq_mode_t i_mode;//inquiry模式 
    uint8_t inq_len;//
    uint8_t num_rsps;
} bsp_blue_handle_t;

void bsp_blue_init(bsp_blue_handle_t* param);
#elif CONFIG_ENABLE_DEVICE_HAL_BLUE
    #error "CONFIG_ENABLE_DEVICE_HAL_BLUE requires an ESP target with Bluetooth Classic support."
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif
