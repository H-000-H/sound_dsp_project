#include "cloud_service.hpp"

#include "capability/led_engine.hpp"
#include "config.hpp"
#include "device.h"
#include "event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.hpp"
#include "system_log.hpp"
#include "system_wdt.hpp"

#include <cstdio>
#include <cstring>

#include <esp_event.h>
#include <esp_wifi.h>

#ifdef CONFIG_MBEDTLS_MEMORY_BUFFER_ALLOC
#include "mbedtls/memory_buffer_alloc.h"
#endif

static constexpr const char* kTag = "CloudService";

/*
 * TLS 内存碎片防御 (IEC 62304 §5.7 连续内存预留):
 *   MbedTLS TLS 握手需 30-40KB 连续内存.
 *   系统运行数天后 LVGL/音频碎片可能使最大连续块 < 15KB → 断连不可恢复.
 *   此 BSS 数组在编译时锁定 40KB 内部 SRAM 连续区域,
 *   专供 ESP-MQTT 内部 mbedtls 握手使用.
 *   若不需要 TLS (mqtt:// 非 mqtts://), 可定义 SKIP_TLS_RESERVE 回收.
 */
#ifndef SKIP_TLS_RESERVE
static uint8_t s_tls_heap_reserve[40960] __attribute__((aligned(4)));
#endif

static CloudService* s_cloud_instance = nullptr;

static void cloud_mqtt_event(MqttEvent event, const char* topic, const uint8_t* data, uint16_t len, void* user_ctx)
{
    (void)user_ctx;
    (void)data;
    (void)len;

    if (event == MqttEvent::Connected)
    {
        SYS_LOGI(kTag, "mqtt connected");
        MqttClient::get_instance().subscribe("attributes/push", 0);
        return;
    }

    if (event == MqttEvent::ReceivedData)
    {
        SYS_LOGI(kTag, "mqtt data topic=%s", topic ? topic : "");
    }

    if (event == MqttEvent::Disconnected)
    {
        SYS_LOGW(kTag, "mqtt disconnected");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        SYS_LOGW(kTag, "WiFi disconnected — tearing down MQTT");
        MqttClient::get_instance().disconnect();
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        auto* ip_event = (ip_event_got_ip_t*)event_data;
        SYS_LOGI(kTag, "WiFi got IP: " IPSTR, IP2STR(&ip_event->ip_info.ip));

        if (s_cloud_instance)
        {
            s_cloud_instance->mqtt_init_and_connect();
        }
    }
}

CloudService& CloudService::getInstance()
{
    static CloudService service;
    return service;
}

bool CloudService::init()
{
    if (m_inited) return true;

    s_cloud_instance = this;

    /*
     * 将预分配的 40KB BSS 数组注册为 MbedTLS 专属内存池.
     * TLS 握手永远从这块连续内存分配, 不受 UI/Audio 碎片影响.
     */
#ifdef CONFIG_MBEDTLS_MEMORY_BUFFER_ALLOC
#ifndef SKIP_TLS_RESERVE
    mbedtls_memory_buffer_alloc_init(s_tls_heap_reserve, sizeof(s_tls_heap_reserve));
    SYS_LOGI(kTag, "mbedtls memory pool init: %u bytes", (unsigned)sizeof(s_tls_heap_reserve));
#endif
#endif

    m_mqtt_cfg.broker_uri = CONFIG_THINGSCLOUD_MQTT_URI;
    m_mqtt_cfg.client_id = CONFIG_THINGSCLOUD_MQTT_CLIENT_ID;
    m_mqtt_cfg.username = CONFIG_THINGSCLOUD_MQTT_USERNAME;
    m_mqtt_cfg.password = CONFIG_THINGSCLOUD_MQTT_PASSWORD;
    m_mqtt_cfg.keepalive_seconds = 60;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t sta_cfg = {};
    strncpy((char*)sta_cfg.sta.ssid, CONFIG_THINGSCLOUD_WIFI_SSID, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char*)sta_cfg.sta.password, CONFIG_THINGSCLOUD_WIFI_PASSWORD, sizeof(sta_cfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr);

    esp_wifi_start();

    led_engine_init_struct(&m_led);
    m_led.init(&m_led);

    m_inited = true;
    EventBus::getInstance().post(SystemEvent::CloudReady);
    return true;
}

void CloudService::mqtt_init_and_connect()
{
    auto& mqtt = MqttClient::get_instance();
    mqtt.set_config(m_mqtt_cfg);
    mqtt.init(cloud_mqtt_event, nullptr);
    mqtt.connect();
}

void CloudService::wifi_connect()
{
    esp_wifi_connect();
}

bool CloudService::start()
{
    if (m_started) return true;
    if (!m_inited && !init()) return false;

    esp_wifi_connect();
    mqtt_init_and_connect();
    m_started = true;
    return true;
}

void CloudService::stop()
{
    if (!m_inited) return;

    MqttClient::get_instance().disconnect();
    esp_wifi_disconnect();
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);

    m_started = false;
    m_inited = false;
}

void CloudService::suspend()
{
    m_started = false;
}

void CloudService::resume()
{
    if (m_started) return;
    if (!m_inited) return;
    m_started = true;
}

void CloudService::run()
{
    start();

    uint32_t count = 0;
    while (true)
    {
        system_wdt_feed();
        system_wdt_feed_rtc();
        system_wdt_stack_check_all();
        vTaskDelay(pdMS_TO_TICKS(30000));

        auto& mqtt = MqttClient::get_instance();
        if (mqtt.is_connected())
        {
            const float temperature = 25.0f + (count % 10);
            const uint32_t humidity = 50 + (count % 20);
            char message[128] = {};
            snprintf(message, sizeof(message), "{\"temperature\":%.1f,\"humidity\":%lu}", temperature, humidity);
            mqtt.publish("attributes", reinterpret_cast<const uint8_t*>(message), strlen(message), 0);
        }

        count++;
    }
}