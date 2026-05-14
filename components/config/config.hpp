#pragma once

#include "soc/soc_caps.h"

/*================================================================================*/
/* 物理按键 GPIO 配置                                                              */
/* 实际硬件：NEXT=16, PREV=17, ENTER=3, ESC=46                                      */
/*================================================================================*/
#ifndef CONFIG_LVGL_KEY_NEXT_GPIO
    #define CONFIG_LVGL_KEY_NEXT_GPIO 16
#endif

#ifndef CONFIG_LVGL_KEY_PREV_GPIO
    #define CONFIG_LVGL_KEY_PREV_GPIO 17
#endif

#ifndef CONFIG_LVGL_KEY_ENTER_GPIO
    #define CONFIG_LVGL_KEY_ENTER_GPIO 3
#endif

#ifndef CONFIG_LVGL_KEY_ESC_GPIO
    #define CONFIG_LVGL_KEY_ESC_GPIO 46
#endif

#ifndef CONFIG_LVGL_KEY_DEBOUNCE_MS
    #define CONFIG_LVGL_KEY_DEBOUNCE_MS 50
#endif

/*================================================================================*/
/* LCD — ST7789 引脚配置                                                           */
/*================================================================================*/
#ifndef CONFIG_BSP_LCD_ST7789_PIN_CLK
    #define CONFIG_BSP_LCD_ST7789_PIN_CLK 4
#endif

#ifndef CONFIG_BSP_LCD_ST7789_PIN_MOSI
    #define CONFIG_BSP_LCD_ST7789_PIN_MOSI 5
#endif

#ifndef CONFIG_BSP_LCD_ST7789_PIN_DC
    #define CONFIG_BSP_LCD_ST7789_PIN_DC 6
#endif

#ifndef CONFIG_BSP_LCD_ST7789_PIN_RST
    #define CONFIG_BSP_LCD_ST7789_PIN_RST 7
#endif

#ifndef CONFIG_BSP_LCD_ST7789_PIN_BLK
    #define CONFIG_BSP_LCD_ST7789_PIN_BLK 15
#endif

#ifndef CONFIG_BSP_LCD_ST7789_BACKLIGHT_ACTIVE_HIGH
    #define CONFIG_BSP_LCD_ST7789_BACKLIGHT_ACTIVE_HIGH 1
#endif

/*================================================================================*/
/* RGB LED                                                                         */
/*================================================================================*/
#ifndef CONFIG_BSP_RGB_LED_GPIO
    #define CONFIG_BSP_RGB_LED_GPIO 48
#endif

#ifndef CONFIG_BSP_RGB_LED_DEFAULT_BRIGHTNESS
    #define CONFIG_BSP_RGB_LED_DEFAULT_BRIGHTNESS 28
#endif

#ifndef CONFIG_BSP_RGB_LED_RMT_RESOLUTION_HZ
    #define CONFIG_BSP_RGB_LED_RMT_RESOLUTION_HZ (10 * 1000 * 1000)
#endif

/*================================================================================*/
/* 蓝牙                                                                             */
/*================================================================================*/
#ifndef CONFIG_BSP_BLUE_DEFAULT_INQ_LEN
    #define CONFIG_BSP_BLUE_DEFAULT_INQ_LEN 10
#endif

#ifndef CONFIG_BSP_BLUE_DEFAULT_NUM_RSPS
    #define CONFIG_BSP_BLUE_DEFAULT_NUM_RSPS 0
#endif

/*================================================================================*/
/* RTC / NTP                                                                       */
/*================================================================================*/
#ifndef CONFIG_BSP_RTC_TIMEZONE
    #define CONFIG_BSP_RTC_TIMEZONE "CST-8"
#endif

#ifndef CONFIG_BSP_RTC_NTP_SERVER_0
    #define CONFIG_BSP_RTC_NTP_SERVER_0 "ntp.aliyun.com"
#endif

#ifndef CONFIG_BSP_RTC_NTP_SERVER_1
    #define CONFIG_BSP_RTC_NTP_SERVER_1 "time.pool.aliyun.com"
#endif

#ifndef CONFIG_BSP_RTC_NTP_SERVER_2
    #define CONFIG_BSP_RTC_NTP_SERVER_2 "cn.pool.ntp.org"
#endif

/*================================================================================*/
/* 模块开关 — 根据芯片能力自动启用                                                    */
/*================================================================================*/
#ifndef CONFIG_ENABLE_DEVICE_HAL_BUTTON
    #define CONFIG_ENABLE_DEVICE_HAL_BUTTON 1
#endif

#ifndef CONFIG_ENABLE_DEVICE_HAL_BLUE
    #if defined(SOC_BT_CLASSIC_SUPPORTED) && SOC_BT_CLASSIC_SUPPORTED
        #define CONFIG_ENABLE_DEVICE_HAL_BLUE 1
    #else
        #define CONFIG_ENABLE_DEVICE_HAL_BLUE 0
    #endif
#endif

#ifndef CONFIG_ENABLE_DEVICE_HAL_WIFI
    #if defined(SOC_WIFI_SUPPORTED) && SOC_WIFI_SUPPORTED
        #define CONFIG_ENABLE_DEVICE_HAL_WIFI 1
    #else
        #define CONFIG_ENABLE_DEVICE_HAL_WIFI 0
    #endif
#endif

#ifndef CONFIG_ENABLE_DEVICE_HAL_LED
    #define CONFIG_ENABLE_DEVICE_HAL_LED 1
#endif

#ifndef CONFIG_ENABLE_DEVICE_HAL_LCD
    #define CONFIG_ENABLE_DEVICE_HAL_LCD 1
#endif

#ifndef CONFIG_ENABLE_DEVICE_HAL_TIME
    #define CONFIG_ENABLE_DEVICE_HAL_TIME 1
#endif

#ifndef CONFIG_ENABLE_LCD_SINGLE_MODE
    #define CONFIG_ENABLE_LCD_SINGLE_MODE 1
#endif

#ifndef CONFIG_ENABLE_LCD_MUX_MODE
    #define CONFIG_ENABLE_LCD_MUX_MODE 0
#endif

#ifndef CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE
    #define CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE 1
#endif

#ifndef CONFIG_ENABLE_LCD_SUB_DEVICE
    #define CONFIG_ENABLE_LCD_SUB_DEVICE 0
#endif

#ifndef CONFIG_ENABLE_LED_SINGLE_MODE
    #define CONFIG_ENABLE_LED_SINGLE_MODE 1
#endif

#ifndef CONFIG_ENABLE_LED_MAIN_WS2812
    #define CONFIG_ENABLE_LED_MAIN_WS2812 1
#endif

/*--------------------------------------------------------------------------------*/
/* BSP 模块开关 — BSP 头/源文件用 #if CONFIG_ENABLE_BSP_* 守卫，定义了才可见类型     */
/*--------------------------------------------------------------------------------*/
#ifndef CONFIG_ENABLE_BSP_GPIO
    #define CONFIG_ENABLE_BSP_GPIO 1
#endif

#ifndef CONFIG_ENABLE_BSP_RGB_LED
    #define CONFIG_ENABLE_BSP_RGB_LED 1
#endif

#ifndef CONFIG_ENABLE_BSP_SPI
    #define CONFIG_ENABLE_BSP_SPI 1
#endif

#ifndef CONFIG_ENABLE_BSP_I2S
    #define CONFIG_ENABLE_BSP_I2S 1
#endif

#ifndef CONFIG_ENABLE_BSP_PWM
    #define CONFIG_ENABLE_BSP_PWM 1
#endif

#ifndef CONFIG_ENABLE_BSP_LCD_ST7789
    #define CONFIG_ENABLE_BSP_LCD_ST7789 1
#endif

#ifndef CONFIG_ENABLE_BSP_WIFI
    #if defined(SOC_WIFI_SUPPORTED) && SOC_WIFI_SUPPORTED
        #define CONFIG_ENABLE_BSP_WIFI 1
    #else
        #define CONFIG_ENABLE_BSP_WIFI 0
    #endif
#endif

#ifndef CONFIG_ENABLE_BSP_TCP
    #define CONFIG_ENABLE_BSP_TCP 1
#endif

#ifndef CONFIG_ENABLE_BSP_MQTT
    #define CONFIG_ENABLE_BSP_MQTT 1
#endif

#ifndef CONFIG_ENABLE_BSP_BLUE
    #if defined(SOC_BT_CLASSIC_SUPPORTED) && SOC_BT_CLASSIC_SUPPORTED
        #define CONFIG_ENABLE_BSP_BLUE 1
    #else
        #define CONFIG_ENABLE_BSP_BLUE 0
    #endif
#endif

#ifndef CONFIG_ENABLE_BSP_RTC
    #define CONFIG_ENABLE_BSP_RTC 1
#endif

/*================================================================================*/
/* ThingsCloud 配置                                                                 */
/*================================================================================*/
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

/*================================================================================*/
/* I2S 音频                                                                         */
/*================================================================================*/
#ifndef CONFIG_AUDIO_I2S_BCK_PIN
    #define CONFIG_AUDIO_I2S_BCK_PIN 41
#endif

#ifndef CONFIG_AUDIO_I2S_WS_PIN
    #define CONFIG_AUDIO_I2S_WS_PIN 42
#endif

#ifndef CONFIG_AUDIO_I2S_DOUT_PIN
    #define CONFIG_AUDIO_I2S_DOUT_PIN 40
#endif

/*================================================================================*/
/* LLM API                                                                         */
/*================================================================================*/
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

/*================================================================================*/
/* Service Factory — 工厂模式总开关                                                */
/*================================================================================*/
#ifndef CONFIG_ENABLE_SERVICE_FACTORY
    #define CONFIG_ENABLE_SERVICE_FACTORY 1
#endif

#ifndef CONFIG_ENABLE_SERVICE_TCP
    #define CONFIG_ENABLE_SERVICE_TCP 1
#endif

#ifndef CONFIG_ENABLE_SERVICE_MQTT
    #define CONFIG_ENABLE_SERVICE_MQTT 1
#endif
