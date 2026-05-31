#include "mqtt_client.hpp"
#if CONFIG_ENABLE_SERVICE_MQTT

#include <atomic>
#include <cstring>

#include "esp_log.h"
#include "mqtt_client.h"

struct MqttClient::Impl
{
    esp_mqtt_client_handle_t client = nullptr;
    std::atomic<bool> is_connected{false};
    MqttConfig cfg;
};

static const char* kTag = "MqttClient";

static MqttEvent convert_event(esp_mqtt_event_id_t id)
{
    switch (id)
    {
        case MQTT_EVENT_CONNECTED:    return MqttEvent::Connected;
        case MQTT_EVENT_DISCONNECTED: return MqttEvent::Disconnected;
        case MQTT_EVENT_SUBSCRIBED:   return MqttEvent::Subscribed;
        case MQTT_EVENT_UNSUBSCRIBED: return MqttEvent::Unsubscribed;
        case MQTT_EVENT_PUBLISHED:    return MqttEvent::Published;
        case MQTT_EVENT_DATA:         return MqttEvent::ReceivedData;
        case MQTT_EVENT_ERROR:        return MqttEvent::Error;
        default:                      return MqttEvent::Error;
    }
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    (void)base;
    auto* self = static_cast<MqttClient*>(handler_args);
    if (!self) return;

    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    if (event_id == MQTT_EVENT_CONNECTED)
    {
        self->dispatch_event(MqttEvent::Connected, nullptr, nullptr, 0);
    }
    else if (event_id == MQTT_EVENT_DISCONNECTED)
    {
        self->dispatch_event(MqttEvent::Disconnected, nullptr, nullptr, 0);
    }
    else if (event_id == MQTT_EVENT_DATA)
    {
        char topic_buf[256] = {};
        int tlen = event->topic_len < (int)sizeof(topic_buf) - 1 ? event->topic_len : (int)sizeof(topic_buf) - 1;
        memcpy(topic_buf, event->topic, tlen);
        topic_buf[tlen] = '\0';
        self->dispatch_event(MqttEvent::ReceivedData, topic_buf, (const uint8_t*)event->data, event->data_len);
    }
    else if (event_id == MQTT_EVENT_ERROR)
    {
        ESP_LOGW(kTag, "MQTT error, type=%d", event->error_handle ? event->error_handle->error_type : -1);
        self->dispatch_event(MqttEvent::Error, nullptr, nullptr, 0);
    }
    else
    {
        self->dispatch_event(convert_event((esp_mqtt_event_id_t)event_id), nullptr, nullptr, 0);
    }
}

MqttClient::MqttClient() : m_impl(new Impl) {}

MqttClient::~MqttClient()
{
    delete m_impl;
}

MqttClient& MqttClient::get_instance()
{
    static MqttClient instance;
    return instance;
}

void MqttClient::set_config(const MqttConfig& cfg)
{
    m_impl->cfg = cfg;
}

void MqttClient::init(event_callback_t cb, void* user_ctx)
{
    if (m_inited) return;
    m_user_cb = cb;
    m_user_ctx = user_ctx;

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = m_impl->cfg.broker_uri;
    mqtt_cfg.credentials.client_id = m_impl->cfg.client_id;
    mqtt_cfg.credentials.username = m_impl->cfg.username;
    mqtt_cfg.credentials.authentication.password = m_impl->cfg.password;
    mqtt_cfg.session.keepalive = m_impl->cfg.keepalive_seconds;

    m_impl->client = esp_mqtt_client_init(&mqtt_cfg);
    if (m_impl->client)
    {
        esp_mqtt_client_register_event(m_impl->client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, this);
    }
    m_inited = true;
}

void MqttClient::connect()
{
    if (!m_impl->client) return;
    esp_mqtt_client_start(m_impl->client);
}

void MqttClient::disconnect()
{
    if (!m_impl->client) return;
    esp_mqtt_client_stop(m_impl->client);
    esp_mqtt_client_destroy(m_impl->client);
    m_impl->client = nullptr;
    m_inited = false;
}

bool MqttClient::is_connected() const
{
    return m_impl->is_connected;
}

int MqttClient::publish(const char* topic, const uint8_t* data, uint16_t len, uint8_t qos)
{
    if (!m_impl->client) return -1;
    return esp_mqtt_client_publish(m_impl->client, topic, (const char*)data, len, qos, 0);
}

int MqttClient::subscribe(const char* topic, uint8_t qos)
{
    if (!m_impl->client) return -1;
    return esp_mqtt_client_subscribe(m_impl->client, topic, qos);
}

int MqttClient::unsubscribe(const char* topic)
{
    if (!m_impl->client) return -1;
    return esp_mqtt_client_unsubscribe(m_impl->client, topic);
}

void MqttClient::dispatch_event(MqttEvent event, const char* topic, const uint8_t* data, uint16_t len)
{
    if (event == MqttEvent::Connected)   m_impl->is_connected = true;
    if (event == MqttEvent::Disconnected) m_impl->is_connected = false;
    if (m_user_cb)
    {
        m_user_cb(event, topic, data, len, m_user_ctx);
    }
}
#endif
