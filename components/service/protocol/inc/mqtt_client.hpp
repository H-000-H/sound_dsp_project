#pragma once

#include "config.hpp"
#include <cstdint>

#if CONFIG_ENABLE_SERVICE_MQTT
enum class MqttEvent
{
    Connected,
    Disconnected,
    Subscribed,
    Unsubscribed,
    Published,
    ReceivedData,
    Error,
};

struct MqttConfig
{
    const char* broker_uri = nullptr;
    const char* client_id = nullptr;
    const char* username = nullptr;
    const char* password = nullptr;
    uint16_t keepalive_seconds = 60;
};

class MqttClient
{
public:
    using event_callback_t = void (*)(MqttEvent event,
                                      const char* topic,
                                      const uint8_t* data,
                                      uint16_t len,
                                      void* user_ctx);

    static MqttClient& get_instance();

    void set_config(const MqttConfig& cfg);
    void init(event_callback_t cb = nullptr, void* user_ctx = nullptr);
    void connect();
    void disconnect();
    bool is_connected() const;
    bool is_inited() const { return m_inited; }
    int publish(const char* topic, const uint8_t* data, uint16_t len, uint8_t qos = 0);
    int subscribe(const char* topic, uint8_t qos = 0);
    int unsubscribe(const char* topic);

    void dispatch_event(MqttEvent event, const char* topic, const uint8_t* data, uint16_t len);

private:
    MqttClient();
    ~MqttClient();
    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    struct Impl;
    Impl* m_impl;
    event_callback_t m_user_cb = nullptr;
    void* m_user_ctx = nullptr;
    bool m_inited = false;
};
#endif
