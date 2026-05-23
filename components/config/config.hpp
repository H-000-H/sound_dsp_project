#pragma once

/*================================================================================*/
/* 精简编译时配置 — 硬件引脚配置已全部移至 board/board.dts                        */
/*================================================================================*/

/*--------------------------------------------------------------------------------*/
/* Service 开关                                                                  */
/*--------------------------------------------------------------------------------*/
#ifndef CONFIG_ENABLE_SERVICE_TCP
    #define CONFIG_ENABLE_SERVICE_TCP 1
#endif

#ifndef CONFIG_ENABLE_SERVICE_MQTT
    #define CONFIG_ENABLE_SERVICE_MQTT 1
#endif

#ifndef CONFIG_ENABLE_AUDIO_DRIVER
    #define CONFIG_ENABLE_AUDIO_DRIVER 1
#endif

/*--------------------------------------------------------------------------------*/
/* ThingsCloud 云端凭据                                                           */
/*--------------------------------------------------------------------------------*/
#ifndef CONFIG_THINGSCLOUD_WIFI_SSID
    #define CONFIG_THINGSCLOUD_WIFI_SSID "plus"
#endif

#ifndef CONFIG_THINGSCLOUD_WIFI_PASSWORD
    #define CONFIG_THINGSCLOUD_WIFI_PASSWORD "22334455hh"
#endif

#ifndef CONFIG_THINGSCLOUD_MQTT_URI
    #define CONFIG_THINGSCLOUD_MQTT_URI "mqtt://gz-4-mqtt.iot-api.com:1883"
#endif

#ifndef CONFIG_THINGSCLOUD_MQTT_CLIENT_ID
    #define CONFIG_THINGSCLOUD_MQTT_CLIENT_ID "wpzbaryy"
#endif

#ifndef CONFIG_THINGSCLOUD_MQTT_USERNAME
    #define CONFIG_THINGSCLOUD_MQTT_USERNAME "33hsg6n6aygssr8j"
#endif

#ifndef CONFIG_THINGSCLOUD_MQTT_PASSWORD
    #define CONFIG_THINGSCLOUD_MQTT_PASSWORD "Ae0toMoRrM"
#endif

#ifndef CONFIG_THINGSCLOUD_WIFI_TIMEOUT_MS
    #define CONFIG_THINGSCLOUD_WIFI_TIMEOUT_MS 10000
#endif

/*--------------------------------------------------------------------------------*/
/* LLM API                                                                       */
/*--------------------------------------------------------------------------------*/
#ifndef CONFIG_LLM_API_ENABLE
    #define CONFIG_LLM_API_ENABLE 0
#endif

#ifndef CONFIG_LLM_API_BASE_URL
    #define CONFIG_LLM_API_BASE_URL "https://api.deepseek.com/v1"
#endif

#ifndef CONFIG_LLM_API_KEY
    #define CONFIG_LLM_API_KEY ""
#endif

#ifndef CONFIG_LLM_API_MODEL
    #define CONFIG_LLM_API_MODEL "deepseek-chat"
#endif

#ifndef CONFIG_LLM_API_TIMEOUT_MS
    #define CONFIG_LLM_API_TIMEOUT_MS 30000
#endif
