#include "cloud_service.hpp"

#include "config.hpp"
#include "device.h"
#include "event_bus.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.hpp"
#include "system_log.hpp"
#include "ws2812_driver.h"

#include <cstdio>
#include <cstring>

#include <esp_event.h>
#include <esp_netif.h>
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
    if (m_state != ModuleState::Created && m_state != ModuleState::Stopped)
    {
        return true;
    }

    /* ── WiFi init (STA mode) ── */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t sta_cfg = {};
    strncpy((char*)sta_cfg.sta.ssid, CONFIG_THINGSCLOUD_WIFI_SSID, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char*)sta_cfg.sta.password, CONFIG_THINGSCLOUD_WIFI_PASSWORD, sizeof(sta_cfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_start();

    /* ── RGB LED init ── */
    device_t* led_dev = device_find("rgb_led0");
    if (led_dev)
    {
        ws2812_init(led_dev);
    }

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

    m_state = ModuleState::Initialized;
    EventBus::getInstance().post(SystemEvent::CloudReady);
    return true;
}

bool CloudService::start()
{
    if (m_state == ModuleState::Created && !init())
    {
        return false;
    }

    esp_wifi_connect();
    MqttClient::get_instance().connect();
    m_state = ModuleState::Started;
    return true;
}

void CloudService::stop()
{
    m_state = ModuleState::Stopped;
}

void CloudService::suspend()
{
    m_state = ModuleState::Suspended;
}

void CloudService::resume()
{
    m_state = ModuleState::Started;
}

ModuleState CloudService::state() const
{
    return m_state;
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
