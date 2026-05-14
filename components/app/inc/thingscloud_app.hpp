#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

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

    /** 启动 WiFi 连接（异步），连接成功后自动启动 MQTT（受 mqtt_auto 控制）*/
    void start();
    /** 断开 WiFi 并终止 MQTT 任务 */
    void stop();

    bool is_wifi_connected() const { return m_wifi_connected; }

    /** 手动启停 MQTT（由 UI 设置页调用）*/
    void start_mqtt();
    void stop_mqtt();

    /** MQTT 自动连接开关 — 关闭时 WiFi 连上后也不会启动 MQTT */
    void set_mqtt_auto(bool enable) { m_mqtt_auto = enable; }
    bool is_mqtt_auto() const { return m_mqtt_auto; }

    /** 获取 WiFi 事件组（供内部 wifi_connect_task 使用） */
    EventGroupHandle_t wifi_evt() {
        if (!m_wifi_evt) m_wifi_evt = xEventGroupCreate();
        return m_wifi_evt;
    }

    /** WiFi 连接成功后的回调（由内部 wifi_connect_task 调用） */
    void on_wifi_connected();
    /** WiFi 意外断连回调（由 BSP 断连回调触发） */
    void on_wifi_disconnected();
    /** 连接超时或被取消时清除 connecting 标志 */
    void on_connect_done() { m_wifi_connecting = false; }

private:
    ThingsCloudApp() = default;
    ~ThingsCloudApp() = default;
    ThingsCloudApp(const ThingsCloudApp&) = delete;
    ThingsCloudApp& operator=(const ThingsCloudApp&) = delete;

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
    TaskHandle_t m_mqtt_task = nullptr;
    volatile bool m_wifi_connected = false;
    volatile bool m_wifi_connecting = false;
    bool m_mqtt_auto = true;   /* 默认开启（兼容已有行为）*/
    EventGroupHandle_t m_wifi_evt = nullptr;   /* wifi_connect_task 事件组，支持超时/取消 */
};
