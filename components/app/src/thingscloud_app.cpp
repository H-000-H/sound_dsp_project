#include "thingscloud_app.hpp"
#include "ui/screen/inc/status_bar.hpp"

#include <cstring>

#include <cJSON.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "device.h"
#include "mqtt_client.hpp"
#include "ws2812_driver.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_CANCEL_BIT    BIT1

/* ── 颜色常量 (RGB 24-bit) ── */
static constexpr uint32_t COLOR_BLACK   = 0x000000;
static constexpr uint32_t COLOR_RED     = 0xFF0000;
static constexpr uint32_t COLOR_GREEN   = 0x00FF00;
static constexpr uint32_t COLOR_BLUE    = 0x0000FF;
static constexpr uint32_t COLOR_WHITE   = 0xFFFFFF;
static constexpr uint32_t COLOR_YELLOW  = 0xFFFF00;
static constexpr uint32_t COLOR_CYAN    = 0x00FFFF;
static constexpr uint32_t COLOR_MAGENTA = 0xFF00FF;
static constexpr uint32_t COLOR_PINK    = 0xFFC0CB;

static device_t* s_led_dev = nullptr;

namespace
{

bool parse_push_attr(const uint8_t* data, uint16_t len, thingscloud_attr_t* out)
{
    memset(out, 0, sizeof(thingscloud_attr_t));
    char json_str[256] = {0};
    int copy_len = len < (int)sizeof(json_str) - 1 ? len : (int)sizeof(json_str) - 1;
    memcpy(json_str, data, copy_len);
    json_str[copy_len] = '\0';

    cJSON* root = cJSON_Parse(json_str);
    if (!root)
    {
        return false;
    }

    cJSON* rgb_led = cJSON_GetObjectItem(root, "led_switch");
    if (rgb_led && cJSON_IsBool(rgb_led))
    {
        out->has_rgb_led = true;
        out->rgb_led_on = cJSON_IsTrue(rgb_led);
    }

    cJSON* color = cJSON_GetObjectItem(root, "RGB_color");
    if (color && cJSON_IsString(color))
    {
        out->has_color = true;
        strncpy(out->color, color->valuestring, sizeof(out->color) - 1);
    }

    cJSON* brightness = cJSON_GetObjectItem(root, "brightness");
    if (brightness && cJSON_IsNumber(brightness))
    {
        out->has_brightness = true;
        out->brightness = (int)brightness->valuedouble;
    }

    cJSON_Delete(root);
    return true;
}

uint32_t color_name_to_u32(const char* name)
{
    if (!name) return COLOR_BLACK;
    if (strcmp(name, "red") == 0) return COLOR_RED;
    if (strcmp(name, "green") == 0) return COLOR_GREEN;
    if (strcmp(name, "blue") == 0) return COLOR_BLUE;
    if (strcmp(name, "white") == 0) return COLOR_WHITE;
    if (strcmp(name, "yellow") == 0) return COLOR_YELLOW;
    if (strcmp(name, "cyan") == 0) return COLOR_CYAN;
    if (strcmp(name, "magenta") == 0) return COLOR_MAGENTA;
    if (strcmp(name, "pink") == 0) return COLOR_PINK;
    if (strcmp(name, "black") == 0) return COLOR_BLACK;
    return COLOR_BLACK;
}

static void set_led_rgb(uint32_t color)
{
    if (!s_led_dev) return;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    ws2812_set_color(s_led_dev, r, g, b);
}

static void on_wifi_got_ip(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    EventGroupHandle_t evt_group = (EventGroupHandle_t)arg;
    xEventGroupSetBits(evt_group, WIFI_CONNECTED_BIT);
}

}  // 匿名命名空间

/*====================================================================*/
/*  MQTT 事件回调                                                     */
/*====================================================================*/
static void mqtt_event_cb(MqttEvent event, const char* topic, const uint8_t* data, uint16_t len, void* user_ctx)
{
    (void)user_ctx;
    auto& mqtt = MqttClient::get_instance();

    if (event == MqttEvent::Connected)
    {
        ESP_LOGI("MQTT", "Connected");
        mqtt.subscribe("attributes/push", 0);
        mqtt.subscribe("command/send/+", 0);
        mqtt.subscribe("attributes/response", 0);
        return;
    }

    if (event == MqttEvent::Disconnected)
    {
        ESP_LOGI("MQTT", "Disconnected");
        return;
    }

    if (event != MqttEvent::ReceivedData)
    {
        return;
    }

    ESP_LOGI("MQTT", "RX topic=%s", topic ? topic : "");
    ESP_LOGI("MQTT", "RX data=%.*s", len, data);

    if (!topic || strcmp(topic, "attributes/push") != 0)
    {
        return;
    }

    thingscloud_attr_t attr;
    if (!parse_push_attr(data, len, &attr))
    {
        return;
    }

    if (attr.has_rgb_led)
    {
        if (attr.rgb_led_on)
        {
            ESP_LOGI("MQTT", "RGB LED ON (white)");
            ws2812_set_brightness(s_led_dev, 255);
            set_led_rgb(COLOR_WHITE);
        }
        else
        {
            ESP_LOGI("MQTT", "RGB LED OFF (black)");
            ws2812_set_brightness(s_led_dev, 0);
            set_led_rgb(COLOR_BLACK);
        }
    }

    if (attr.has_color && strlen(attr.color) > 0)
    {
        ESP_LOGI("MQTT", "RGB color=%s", attr.color);
        set_led_rgb(color_name_to_u32(attr.color));
    }

    if (attr.has_brightness)
    {
        int brightness = attr.brightness;
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;
        ESP_LOGI("MQTT", "RGB brightness=%d", brightness);
        ws2812_set_brightness(s_led_dev, (uint8_t)brightness);
    }
}

ThingsCloudApp& ThingsCloudApp::get_instance()
{
    static ThingsCloudApp instance;
    return instance;
}

void ThingsCloudApp::set_wifi(const char* ssid, const char* pass)
{
    if (ssid) m_ssid = ssid;
    if (pass) m_pass = pass;
}

void ThingsCloudApp::set_mqtt(const char* uri, const char* client_id, const char* user, const char* pass, uint16_t keepalive)
{
    if (uri) m_mqtt_uri = uri;
    if (client_id) m_mqtt_client_id = client_id;
    if (user) m_mqtt_user = user;
    if (pass) m_mqtt_pass = pass;
    m_keepalive = keepalive;
}

void ThingsCloudApp::set_rgb_led(uint32_t gpio)
{
    (void)gpio;
}

void ThingsCloudApp::init()
{
    if (m_ssid.empty()) m_ssid = CONFIG_THINGSCLOUD_WIFI_SSID;
    if (m_pass.empty()) m_pass = CONFIG_THINGSCLOUD_WIFI_PASSWORD;
    if (m_mqtt_uri.empty()) m_mqtt_uri = CONFIG_THINGSCLOUD_MQTT_URI;
    if (m_mqtt_client_id.empty()) m_mqtt_client_id = CONFIG_THINGSCLOUD_MQTT_CLIENT_ID;
    if (m_mqtt_user.empty()) m_mqtt_user = CONFIG_THINGSCLOUD_MQTT_USERNAME;
    if (m_mqtt_pass.empty()) m_mqtt_pass = CONFIG_THINGSCLOUD_MQTT_PASSWORD;

    /* ── LED ── */
    s_led_dev = device_find("rgb_led0");
    if (s_led_dev)
    {
        ws2812_init(s_led_dev);
        ws2812_set_brightness(s_led_dev, 255);
        set_led_rgb(COLOR_BLACK);
    }

    /* ── MQTT ── */
    MqttConfig cfg = {};
    cfg.broker_uri = m_mqtt_uri.c_str();
    cfg.client_id = m_mqtt_client_id.c_str();
    cfg.username = m_mqtt_user.c_str();
    cfg.password = m_mqtt_pass.c_str();
    cfg.keepalive_seconds = m_keepalive;

    auto& mqtt = MqttClient::get_instance();
    mqtt.set_config(cfg);
    mqtt.init(mqtt_event_cb, this);
}

/*====================================================================*/
/*  WiFi / MQTT 启停                                                  */
/*====================================================================*/

static void mqtt_task_entry(void* param)
{
    (void)param;
    auto& mqtt = MqttClient::get_instance();

    mqtt.connect();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

static void wifi_connect_task(void* param)
{
    ThingsCloudApp* app = (ThingsCloudApp*)param;

    {
        EventGroupHandle_t eg = app->wifi_evt();
        xEventGroupClearBits(eg, WIFI_CONNECTED_BIT | WIFI_CANCEL_BIT);
    }

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_got_ip);
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_got_ip, app->wifi_evt())
    );

    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(app->wifi_evt(),
                                           WIFI_CONNECTED_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_THINGSCLOUD_WIFI_TIMEOUT_MS));

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_got_ip);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI("ThingsCloud", "WiFi connected");
        app->on_wifi_connected();
    }
    else if (bits & WIFI_CANCEL_BIT)
    {
        ESP_LOGI("ThingsCloud", "WiFi cancelled by stop()");
        app->on_connect_done();
    }
    else
    {
        ESP_LOGW("ThingsCloud", "WiFi timeout after %d ms", CONFIG_THINGSCLOUD_WIFI_TIMEOUT_MS);
        esp_wifi_disconnect();
        esp_wifi_stop();
        app->on_connect_done();
    }

    vTaskDelete(NULL);
}

void ThingsCloudApp::start()
{
    if (m_wifi_connected)
    {
        ESP_LOGW("ThingsCloud", "WiFi already connected, skipping");
        return;
    }
    if (m_wifi_connecting)
    {
        ESP_LOGW("ThingsCloud", "WiFi already connecting, skipping");
        return;
    }

    if (m_ssid.empty() || m_ssid[0] == '\0')
    {
        ESP_LOGW("ThingsCloud", "WiFi SSID is empty, skip connect");
        return;
    }

    m_wifi_connecting = true;

    xTaskCreatePinnedToCore(wifi_connect_task, "wifi_con", 8*1024, this, 5, NULL, 1);
}

void ThingsCloudApp::on_wifi_connected()
{
    m_wifi_connected = true;
    m_wifi_connecting = false;
    ui_set_wifi_state(true);
    if (m_mqtt_auto)
    {
        start_mqtt();
    } else {
        ESP_LOGI("ThingsCloud", "MQTT auto disabled by settings, skipping");
    }
}

void ThingsCloudApp::on_wifi_disconnected()
{
    m_wifi_connected = false;
    ui_set_wifi_state(false);
    ESP_LOGI("ThingsCloud", "WiFi unexpectedly disconnected, icon hidden");
}

void ThingsCloudApp::stop()
{
    m_wifi_connecting = false;
    stop_mqtt();

    if (m_wifi_evt)
    {
        xEventGroupSetBits(m_wifi_evt, WIFI_CANCEL_BIT);
    }

    esp_wifi_disconnect();
    esp_wifi_stop();

    m_wifi_connected = false;
    ui_set_wifi_state(false);

    ESP_LOGI("ThingsCloud", "WiFi disconnected, MQTT stopped");
}

void ThingsCloudApp::start_mqtt()
{
    if (m_mqtt_task)
    {
        ESP_LOGW("ThingsCloud", "MQTT task already running");
        return;
    }

    auto& mqtt = MqttClient::get_instance();
    if (!mqtt.is_inited())
    {
        MqttConfig cfg = {};
        cfg.broker_uri = m_mqtt_uri.c_str();
        cfg.client_id  = m_mqtt_client_id.c_str();
        cfg.username   = m_mqtt_user.c_str();
        cfg.password   = m_mqtt_pass.c_str();
        cfg.keepalive_seconds = m_keepalive;
        mqtt.set_config(cfg);
        mqtt.init(mqtt_event_cb, this);
    }

    xTaskCreatePinnedToCore(mqtt_task_entry, "mqtt_io", 6*1024, this, 5, &m_mqtt_task, 1);
    ESP_LOGI("ThingsCloud", "MQTT task started");
}

void ThingsCloudApp::stop_mqtt()
{
    if (m_mqtt_task)
    {
        auto& mqtt = MqttClient::get_instance();
        mqtt.disconnect();
        vTaskDelete(m_mqtt_task);
        m_mqtt_task = nullptr;
        ESP_LOGI("ThingsCloud", "MQTT task stopped");
    }
}

void ThingsCloudApp::report(float temp, uint32_t humi)
{
    auto& mqtt = MqttClient::get_instance();
    if (!mqtt.is_connected()) return;
    if (temp == m_last_temp && humi == m_last_humi) return;

    char msg[128];
    snprintf(msg, sizeof(msg), "{\"temperature\":%.1f,\"humidity\":%lu}", temp, humi);
    mqtt.publish("attributes", (const uint8_t*)msg, strlen(msg), 0);
    m_last_temp = temp;
    m_last_humi = humi;
}

void ThingsCloudApp::run(sensor_cb_t cb)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (!cb) continue;

        float temp = 0.0f;
        uint32_t humi = 0;
        cb(&temp, &humi);
        report(temp, humi);
    }
}
