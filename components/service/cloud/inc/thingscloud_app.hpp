#pragma once

#include <cstdint>
#include <cstring>

struct thingscloud_attr_t
{
    bool has_rgb_led;
    bool rgb_led_on;
    bool has_color;
    char color[16];
    bool has_brightness;
    int brightness;
};

class ThingsCloudApp
{
public:
    using sensor_cb_t = void (*)(float* temp, uint32_t* humi);

    static ThingsCloudApp& get_instance();

    void set_wifi(const char* ssid, const char* pass);
    void set_mqtt(const char* uri, const char* client_id, const char* user, const char* pass, uint16_t keepalive = 60);
    void set_rgb_led(uint32_t gpio);
    void init();
    void report(float temp, uint32_t humi);
    void run(sensor_cb_t cb = nullptr);

    void start();
    void stop();

    bool is_wifi_connected() const { return m_wifi_connected; }

    void start_mqtt();
    void stop_mqtt();

    void set_mqtt_auto(bool enable) { m_mqtt_auto = enable; }
    bool is_mqtt_auto() const { return m_mqtt_auto; }

    using wifi_state_cb_t = void (*)(bool connected);
    void set_wifi_state_cb(wifi_state_cb_t cb) { m_wifi_state_cb = cb; }

    void* wifi_evt();
    void on_wifi_connected();
    void on_connect_done() { m_wifi_connecting = false; }

private:
    ThingsCloudApp() = default;
    ~ThingsCloudApp() = default;
    ThingsCloudApp(const ThingsCloudApp&) = delete;
    ThingsCloudApp& operator=(const ThingsCloudApp&) = delete;

    void on_wifi_disconnected();

    /* 固定长度 char 数组替代 std::string — 零堆分配 */
    static constexpr size_t kSsidMax = 33;
    static constexpr size_t kPassMax = 65;
    static constexpr size_t kUriMax = 129;
    static constexpr size_t kClientIdMax = 65;
    static constexpr size_t kUserMax = 65;

    char m_ssid[kSsidMax] = {};
    char m_pass[kPassMax] = {};
    char m_mqtt_uri[kUriMax] = {};
    char m_mqtt_client_id[kClientIdMax] = {};
    char m_mqtt_user[kUserMax] = {};
    char m_mqtt_pass[kPassMax] = {};
    uint16_t m_keepalive = 60;
    uint32_t m_rgb_gpio = 0;
    float m_last_temp = 0.0f;
    uint32_t m_last_humi = 0;
    void* m_mqtt_task = nullptr;
    void* m_wifi_evt = nullptr;
    wifi_state_cb_t m_wifi_state_cb = nullptr;
    volatile bool m_wifi_connected = false;
    volatile bool m_wifi_connecting = false;
    bool m_mqtt_auto = true;
};