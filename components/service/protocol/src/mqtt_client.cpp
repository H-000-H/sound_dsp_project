#include "mqtt_client.hpp"
#if CONFIG_ENABLE_SERVICE_MQTT

extern "C"
{
#include "bsp_mqtt.h"
}

#include <cstring>

struct MqttClient::Impl
{
    bsp_mqtt_handle_t handle = {};
};

static MqttEvent convert_event(bsp_mqtt_event_t event)
{
    switch (event)
    {
        case BSP_MQTT_EVENT_CONNECTED: return MqttEvent::Connected;
        case BSP_MQTT_EVENT_DISCONNECTED: return MqttEvent::Disconnected;
        case BSP_MQTT_EVENT_SUBSCRIBED: return MqttEvent::Subscribed;
        case BSP_MQTT_EVENT_UNSUBSCRIBED: return MqttEvent::Unsubscribed;
        case BSP_MQTT_EVENT_PUBLISHED: return MqttEvent::Published;
        case BSP_MQTT_EVENT_RXDATA: return MqttEvent::ReceivedData;
        case BSP_MQTT_EVENT_ERROR: return MqttEvent::Error;
        default: return MqttEvent::Error;
    }
}

static void bsp_mqtt_event_bridge(bsp_mqtt_event_t event, const char* topic, const uint8_t* data, uint16_t len, void* user_ctx)
{
    auto* self = static_cast<MqttClient*>(user_ctx);
    if (!self)
    {
        return;
    }
    self->dispatch_event(convert_event(event), topic ? topic : "", data, len);
}

MqttClient::MqttClient() : m_impl(new Impl)
{
}

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
    memset(&m_impl->handle.cfg, 0, sizeof(m_impl->handle.cfg));
    if (cfg.broker_uri)
    {
        strncpy(m_impl->handle.cfg.broker_uri, cfg.broker_uri, sizeof(m_impl->handle.cfg.broker_uri) - 1);
    }
    if (cfg.client_id)
    {
        strncpy(m_impl->handle.cfg.client_id, cfg.client_id, sizeof(m_impl->handle.cfg.client_id) - 1);
    }
    if (cfg.username)
    {
        strncpy(m_impl->handle.cfg.username, cfg.username, sizeof(m_impl->handle.cfg.username) - 1);
    }
    if (cfg.password)
    {
        strncpy(m_impl->handle.cfg.password, cfg.password, sizeof(m_impl->handle.cfg.password) - 1);
    }
    m_impl->handle.cfg.keepalive_seconds = cfg.keepalive_seconds;
}

void MqttClient::init(event_callback_t cb, void* user_ctx)
{
    if (m_inited) return;  /* 防止重复 init 泄漏 */
    m_user_cb = cb;
    m_user_ctx = user_ctx;
    bsp_mqtt_init(&m_impl->handle, bsp_mqtt_event_bridge, this);
    m_inited = true;
}

void MqttClient::connect()
{
    bsp_mqtt_connect(&m_impl->handle);
}

void MqttClient::disconnect()
{
    bsp_mqtt_disconnect(&m_impl->handle);
    m_inited = false;
}

bool MqttClient::is_connected() const
{
    return bsp_mqtt_is_connected(&m_impl->handle);
}

int MqttClient::publish(const char* topic, const uint8_t* data, uint16_t len, uint8_t qos)
{
    return bsp_mqtt_publish(&m_impl->handle, topic, data, len, qos);
}

int MqttClient::subscribe(const char* topic, uint8_t qos)
{
    return bsp_mqtt_subscribe(&m_impl->handle, topic, qos);
}

int MqttClient::unsubscribe(const char* topic)
{
    return bsp_mqtt_unsubscribe(&m_impl->handle, topic);
}

void MqttClient::dispatch_event(MqttEvent event, const char* topic, const uint8_t* data, uint16_t len)
{
    if (m_user_cb)
    {
        m_user_cb(event, topic, data, len, m_user_ctx);
    }
}
#endif
