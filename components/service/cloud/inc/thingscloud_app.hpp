#pragma once

#include <cstdint>
#include <functional>
#include <string>

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
    using sensor_cb_t = std::function<void(float* temp, uint32_t* humi)>;

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

    /** 注册 WiFi 状态回调 (由 app 层注册, 解耦 service → app) */
    using wifi_state_cb_t = void (*)(bool connected);
    void set_wifi_state_cb(wifi_state_cb_t cb) { m_wifi_state_cb = cb; }

    /** 内部 WiFi 事件组 (void* 隐藏 FreeRTOS EventGroupHandle_t) */
    void* wifi_evt();
    void on_wifi_connected();
    void on_connect_done() { m_wifi_connecting = false; }

private:
    ThingsCloudApp() = default;
    ~ThingsCloudApp() = default;
    ThingsCloudApp(const ThingsCloudApp&) = delete;
    ThingsCloudApp& operator=(const ThingsCloudApp&) = delete;

    void on_wifi_disconnected();

    std::string m_ssid;
    std::string m_pass;
    std::string m_mqtt_uri;
    std::string m_mqtt_client_id;
    std::string m_mqtt_user;
    std::string m_mqtt_pass;
    uint16_t m_keepalive = 60;
    uint32_t m_rgb_gpio = 0;
    float m_last_temp = 0.0f;
    uint32_t m_last_humi = 0;
    void* m_mqtt_task = nullptr;          /* TaskHandle_t (隐藏 FreeRTOS 细节) */
    void* m_wifi_evt = nullptr;           /* EventGroupHandle_t */
    wifi_state_cb_t m_wifi_state_cb = nullptr;
    volatile bool m_wifi_connected = false;
    volatile bool m_wifi_connecting = false;
    bool m_mqtt_auto = true;
};
