#include "cloud_service.hpp"

#include "capability/led_engine.hpp"
#include "config.hpp"
#include "device.h"
#include "event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.hpp"
#include "system_log.hpp"

#include <cstdio>
#include <cstring>

#include <esp_wifi.h>

static constexpr const char* kTag = "CloudService";

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
}

CloudService& CloudService::getInstance()
{
    static CloudService service;
    return service;
}

bool CloudService::init()
{
    if (m_inited) return true;

    /* ── WiFi init (STA mode) ── */
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t sta_cfg = {};
    strncpy((char*)sta_cfg.sta.ssid, CONFIG_THINGSCLOUD_WIFI_SSID, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char*)sta_cfg.sta.password, CONFIG_THINGSCLOUD_WIFI_PASSWORD, sizeof(sta_cfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();

    /* ── RGB LED init (via capability) ── */
    led_engine_init_struct(&m_led);
    m_led.init(&m_led);

    /* ── MQTT init ── */
    MqttConfig mqtt_config = {};
    mqtt_config.broker_uri = CONFIG_THINGSCLOUD_MQTT_URI;
    mqtt_config.client_id = CONFIG_THINGSCLOUD_MQTT_CLIENT_ID;
    mqtt_config.username = CONFIG_THINGSCLOUD_MQTT_USERNAME;
    mqtt_config.password = CONFIG_THINGSCLOUD_MQTT_PASSWORD;
    mqtt_config.keepalive_seconds = 60;

    auto& mqtt = MqttClient::get_instance();
    mqtt.set_config(mqtt_config);
    mqtt.init(cloud_mqtt_event, this);

    m_inited = true;
    EventBus::getInstance().post(SystemEvent::CloudReady);
    return true;
}

bool CloudService::start()
{
    if (m_started) return true;
    if (!m_inited && !init()) return false;

    esp_wifi_connect();
    MqttClient::get_instance().connect();
    m_started = true;
    return true;
}

void CloudService::stop()
{
    if (!m_inited) return;
    m_started = false;
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
