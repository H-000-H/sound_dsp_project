/*本工厂统一提供各模块实例入口，包含普通驱动、网络服务与蓝牙模块。*/
#pragma once
#include "config.hpp"

#if !CONFIG_ENABLE_SERVICE_FACTORY
    #error "factory.hpp is included while CONFIG_ENABLE_SERVICE_FACTORY is disabled. Remove app/service factory dependencies before turning it off."
#endif

#include "gpio_controller.hpp"
#include "rgb_led.hpp"
#if CONFIG_ENABLE_DEVICE_HAL_LCD
    #include "display_device.hpp"
    #include "st7789.hpp"
#endif
#if CONFIG_ENABLE_DEVICE_HAL_LED
    #include "light_device.hpp"
#endif
#if CONFIG_ENABLE_DEVICE_HAL_TIME
    #include "m_time.hpp"
#endif
#if CONFIG_ENABLE_DEVICE_HAL_BUTTON
    #include "button.hpp"
#endif
#if CONFIG_ENABLE_DEVICE_HAL_WIFI
    #include "wifi_manager.hpp"
#endif
#if CONFIG_ENABLE_SERVICE_TCP
    #include "tcp_client.hpp"
#endif
#if CONFIG_ENABLE_SERVICE_MQTT
    #include "mqtt_client.hpp"
#endif
#if CONFIG_ENABLE_DEVICE_HAL_BLUE
    #include "blue.hpp"
#endif
#if CONFIG_USE_MAX_98357A
    #include "max98357a.hpp"
#endif
/**
 * @brief 屏幕驱动通一调用函数
 * @return 各自屏幕驱动实例指针
 * @note 该函数会根据当前配置的屏幕驱动类型返回对应的实例指针
 * @note SINGLE_SCREEN 为单屏幕模式，MUX_SCREEN 为多屏幕模式 ，默认开启单屏幕模式 
 * @warning 该函数在未开启屏幕驱动时用error宏报错
 */
/*================================================================================*/

namespace factory_config::screen
{
#if CONFIG_ENABLE_DEVICE_HAL_LCD
    #if CONFIG_ENABLE_LCD_SINGLE_MODE == 1 && CONFIG_ENABLE_LCD_MUX_MODE == 1
        #error "CONFIG_ENABLE_LCD_SINGLE_MODE and CONFIG_ENABLE_LCD_MUX_MODE cannot both be enabled!"
    #elif CONFIG_ENABLE_LCD_SINGLE_MODE == 1
    #if (CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE + CONFIG_ENABLE_LCD_MAIN_ST7789_8WIRE + CONFIG_ENABLE_LCD_MAIN_USB_ORANGE_1024X600) != 1
        #error "Exactly one main screen driver must be enabled in single mode!"
    #endif
    #if CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE == 1
        using MainDisplay = St7789Display7Wire;
    #elif CONFIG_ENABLE_LCD_MAIN_ST7789_8WIRE == 1
        using MainDisplay = St7789Display8Wire;
    #elif CONFIG_ENABLE_LCD_MAIN_USB_ORANGE_1024X600 == 1
        using MainDisplay = USB_ORANGE_1024x600;
    #else
        #error "No valid main screen driver selected!"
    #endif

    static inline DisplayDevice* get_device()
    {
        return &MainDisplay::get_instance();
    }
    #elif CONFIG_ENABLE_LCD_MUX_MODE == 1
    #if (CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE + CONFIG_ENABLE_LCD_MAIN_ST7789_8WIRE + CONFIG_ENABLE_LCD_MAIN_USB_ORANGE_1024X600) != 1
        #error "Exactly one main screen driver must be enabled in mux mode!"
    #endif
    #if CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE == 1
        using MainDisplay = St7789Display7Wire;
    #elif CONFIG_ENABLE_LCD_MAIN_ST7789_8WIRE == 1
        using MainDisplay = St7789Display8Wire;
    #elif CONFIG_ENABLE_LCD_MAIN_USB_ORANGE_1024X600 == 1
        using MainDisplay = USB_ORANGE_1024x600;
    #else
        #error "No valid main screen driver selected in mux mode!"
    #endif

    static inline DisplayDevice* get_main_device()
    {
        return &MainDisplay::get_instance();
    }

    #if CONFIG_ENABLE_LCD_SUB_DEVICE == 1
        #if (CONFIG_ENABLE_LCD_SUB_ST7789_7WIRE + CONFIG_ENABLE_LCD_SUB_ST7789_8WIRE + CONFIG_ENABLE_LCD_SUB_USB_ORANGE_1024X600) != 1
            #error "Exactly one sub screen driver must be enabled when sub screen is enabled!"
        #endif
        #if CONFIG_ENABLE_LCD_SUB_ST7789_7WIRE == 1
            using SubDisplay = St7789Display7Wire;
        #elif CONFIG_ENABLE_LCD_SUB_ST7789_8WIRE == 1
            using SubDisplay = St7789Display8Wire;
        #elif CONFIG_ENABLE_LCD_SUB_USB_ORANGE_1024X600 == 1
            using SubDisplay = USB_ORANGE_1024x600;
        #else
            #error "Sub screen is enabled but no valid sub screen driver selected!"
        #endif

        inline DisplayDevice* get_sub_device()
        {
            return &SubDisplay::get_instance();
        }
    #endif
#endif
#endif
}
/*================================================================================*/



/**
 * @brief LED驱动通一调用函数
 * @return 各自LED驱动实例指针
 * @note 该函数会根据当前配置的LED驱动类型返回对应的实例指针
 * @warning 该函数在未开启LED驱动时用error宏报错
 */
/*================================================================================*/
namespace factory_config::led
{
#if CONFIG_ENABLE_DEVICE_HAL_LED
#if CONFIG_ENABLE_LED_SINGLE_MODE == 1 && CONFIG_ENABLE_LED_MUX_MODE == 1
    #error "CONFIG_ENABLE_LED_SINGLE_MODE and CONFIG_ENABLE_LED_MUX_MODE cannot both be enabled!"
#elif CONFIG_ENABLE_LED_SINGLE_MODE == 1
    #if (CONFIG_ENABLE_LED_MAIN_WS2812 + CONFIG_ENABLE_LED_MAIN_GPIO_SINGLE) != 1
        #error "Exactly one single LED driver must be enabled!"
    #endif

    #if CONFIG_ENABLE_LED_MAIN_WS2812 == 1
        using MainLed = RgbLight;
    #elif CONFIG_ENABLE_LED_MAIN_GPIO_SINGLE == 1
        using MainLed = SingleColorLed;
    #else
        #error "No valid main LED driver selected!"
    #endif

    inline LightDevice* get_device()
    {
        return &MainLed::get_instance();
    }
#elif CONFIG_ENABLE_LED_MUX_MODE == 1
    #if (CONFIG_ENABLE_LED_MAIN_WS2812 + CONFIG_ENABLE_LED_MAIN_GPIO_SINGLE) != 1
        #error "Exactly one main LED driver must be enabled in mux mode!"
    #endif

    #if CONFIG_ENABLE_LED_MAIN_WS2812 == 1
        using MainLed = RgbLight;
    #elif CONFIG_ENABLE_LED_MAIN_GPIO_SINGLE == 1
        using MainLed = SingleColorLed;
    #else
        #error "No valid main LED driver selected in mux mode!"
    #endif

    inline LightDevice* get_main_device()
    {
        return &MainLed::get_instance();
    }

    #if CONFIG_ENABLE_LED_SUB_DEVICE == 1
        #if (CONFIG_ENABLE_LED_SUB_WS2812 + CONFIG_ENABLE_LED_SUB_GPIO_SINGLE) != 1
            #error "Exactly one sub LED driver must be enabled when sub LED is enabled!"
        #endif

        #if CONFIG_ENABLE_LED_SUB_WS2812 == 1
            using SubLed = RgbLight;
        #elif CONFIG_ENABLE_LED_SUB_GPIO_SINGLE == 1
            using SubLed = SingleColorLed;
        #else
            #error "Sub LED is enabled but no valid sub LED driver selected!"
        #endif

        inline LightDevice* get_sub_device()
        {
            return &SubLed::get_instance();
        }
    #endif
#endif
#endif
}
/*================================================================================*/

/*以下为时间相关工厂配置*/
/*================================================================================*/
namespace factory_config::time
{
    #if CONFIG_ENABLE_TIME_NTP_SYNC && CONFIG_ENABLE_TIME_MANUAL_SET
    #error "CONFIG_ENABLE_TIME_NTP_SYNC and CONFIG_ENABLE_TIME_MANUAL_SET cannot both be enabled!"
    #endif
#if CONFIG_ENABLE_DEVICE_HAL_TIME
    using Time_Inf = TimeInfo;
    using time = Time;
    inline time* get_time()
    {
        return &time::get_instance();
    }
#endif
}

/*================================================================================*/

/*以下为按键相关工厂配置*/
/*================================================================================*/
namespace factory_config::button
{
#if CONFIG_ENABLE_DEVICE_HAL_BUTTON
    using button = Button;
    
    inline Button* get_button()
    {
        return &Button::get_instance();
    }
#endif
}

/*================================================================================*/

/*以下为网络相关工厂配置*/
/*================================================================================*/
namespace factory_config::network
{
#if CONFIG_ENABLE_DEVICE_HAL_WIFI
    using wifi = WifiManager;

    inline WifiManager* get_wifi()
    {
        return &WifiManager::get_instance();
    }
#endif

#if CONFIG_ENABLE_SERVICE_TCP
    using tcp = TcpClient;

    inline TcpClient* get_tcp()
    {
        return &TcpClient::get_instance();
    }
#endif

#if CONFIG_ENABLE_SERVICE_MQTT
    using mqtt = MqttClient;

    inline MqttClient* get_mqtt()
    {
        return &MqttClient::get_instance();
    }
#endif
}

/*================================================================================*/

/*以下为蓝牙相关工厂配置*/
/*================================================================================*/
namespace factory_config::blue
{
#if CONFIG_ENABLE_DEVICE_HAL_BLUE
    using blue = Blue;

    inline Blue* get_blue()
    {
        return &Blue::get_instance();
    }
#endif
}

/*=================================================================================*/
/*以下为音频相关工厂配置*/
namespace factory_config::audio
{
#if CONFIG_ENABLE_AUDIO_DEVICE == 1
/*   MAX98357A - 启用MAX98357A的SD引脚      */
    #if CONFIG_USE_MAX_98357A == 1
        using audio_device = MAX_98357A;
    #endif
    inline audio_device* get_audio()
    {
        return &audio_device::get_instance();
    }
#endif
}
/*================================================================================*/