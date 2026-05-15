#include "thingscloud_app.hpp"
#include "ui/screen/inc/status_bar.hpp"

#include <cstring>

#include <cJSON.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "factory.hpp"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_CANCEL_BIT    BIT1

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
    if (!name) return black;
    if (strcmp(name, "red") == 0) return red;
    if (strcmp(name, "green") == 0) return green;
    if (strcmp(name, "blue") == 0) return blue;
    if (strcmp(name, "white") == 0) return white;
    if (strcmp(name, "yellow") == 0) return yellow;
    if (strcmp(name, "cyan") == 0) return cyan;
    if (strcmp(name, "magenta") == 0) return magenta;
    if (strcmp(name, "pink") == 0) return pink;
    if (strcmp(name, "black") == 0) return black;
    return black;
}

static void on_wifi_got_ip(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    EventGroupHandle_t evt_group = (EventGroupHandle_t)arg;
    xEventGroupSetBits(evt_group, WIFI_CONNECTED_BIT);
}

}  // 匿名命名空间

/*====================================================================*/
/*  MQTT 事件回调（可复用，init / re-init 都用它）                    */
/*====================================================================*/
static void mqtt_event_cb(MqttEvent event, const char* topic, const uint8_t* data, uint16_t len, void* user_ctx)
{
    (void)user_ctx;
    auto* mqtt = factory_config::network::get_mqtt();
    auto* rgb = static_cast<ColorLightDevice*>(factory_config::led::get_device());

    if (event == MqttEvent::Connected)
    {
        ESP_LOGI("MQTT", "Connected");
        mqtt->subscribe("attributes/push", 0);
        mqtt->subscribe("command/send/+", 0);
        mqtt->subscribe("attributes/response", 0);
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
            rgb->set_brightness(255);
            rgb->set_color(white);
        }
        else
        {
            ESP_LOGI("MQTT", "RGB LED OFF (black)");
            rgb->set_brightness(0);
            rgb->set_color(black);
        }
    }

    if (attr.has_color && strlen(attr.color) > 0)
    {
        ESP_LOGI("MQTT", "RGB color=%s", attr.color);
        rgb->set_color(color_name_to_u32(attr.color));
    }

    if (attr.has_brightness)
    {
        int brightness = attr.brightness;
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;
        ESP_LOGI("MQTT", "RGB brightness=%d", brightness);
        rgb->set_brightness((uint8_t)brightness);
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
    m_rgb_gpio = gpio;
}

void ThingsCloudApp::init()
{
    // 使用 KConfig 默认值，运行时未覆盖则生效
    if (m_ssid.empty()) m_ssid = CONFIG_THINGSCLOUD_WIFI_SSID;
    if (m_pass.empty()) m_pass = CONFIG_THINGSCLOUD_WIFI_PASSWORD;
    if (m_mqtt_uri.empty()) m_mqtt_uri = CONFIG_THINGSCLOUD_MQTT_URI;
    if (m_mqtt_client_id.empty()) m_mqtt_client_id = CONFIG_THINGSCLOUD_MQTT_CLIENT_ID;
    if (m_mqtt_user.empty()) m_mqtt_user = CONFIG_THINGSCLOUD_MQTT_USERNAME;
    if (m_mqtt_pass.empty()) m_mqtt_pass = CONFIG_THINGSCLOUD_MQTT_PASSWORD;

    auto* wifi = factory_config::network::get_wifi();
    wifi->set_ssid((const uint8_t*)m_ssid.c_str());
    wifi->set_sta_password((const uint8_t*)m_pass.c_str());
    wifi->set_wifi_mode(WifiMode::Sta);
    wifi->init();

    /* 注册 WiFi 意外断连回调，用于更新 UI 图标 */
    wifi->set_disconnected_cb([this]()
    {
        this->on_wifi_disconnected();
    });

    /* WiFi 连接由 start() 统一控制，此处仅初始化配置 */

    auto* rgb = factory_config::led::get_device();
    auto* mqtt = factory_config::network::get_mqtt();

    rgb->init(m_rgb_gpio ? m_rgb_gpio : CONFIG_BSP_RGB_LED_GPIO);
    static_cast<ColorLightDevice*>(rgb)->set_brightness(255);
    static_cast<ColorLightDevice*>(rgb)->set_color(black);

    MqttConfig cfg = {};
    cfg.broker_uri = m_mqtt_uri.c_str();
    cfg.client_id = m_mqtt_client_id.c_str();
    cfg.username = m_mqtt_user.c_str();
    cfg.password = m_mqtt_pass.c_str();
    cfg.keepalive_seconds = m_keepalive;
    mqtt->set_config(cfg);

    mqtt->init(mqtt_event_cb, this);

    /* 不再自动 connect —— 由 start() 统一控制 */
}

/*====================================================================*/
/*  WiFi / MQTT 启停                                                  */
/*====================================================================*/

static void mqtt_task_entry(void* param)
{
    (void)param;
    auto* mqtt = factory_config::network::get_mqtt();
    
    mqtt->connect();
    
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(30000));
        /* 可在此添加定期上报等逻辑 */
    }
}

static void wifi_connect_task(void* param)
{
    ThingsCloudApp* app = (ThingsCloudApp*)param;
    auto* wifi = factory_config::network::get_wifi();

    /* 使用 App 级事件组，支持从 stop() 取消 */
    {
        EventGroupHandle_t eg = app->wifi_evt();
        xEventGroupClearBits(eg, WIFI_CONNECTED_BIT | WIFI_CANCEL_BIT);
    }

    /* 先注销再注册，避免 "handler already registered" */
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_got_ip);
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_got_ip, app->wifi_evt())
    );

    /* 注册完 handler 再发起连接，避免 IP 事件在注册前到达 */
    wifi->connect();

    EventBits_t bits = xEventGroupWaitBits(app->wifi_evt(),
                                           WIFI_CONNECTED_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_THINGSCLOUD_WIFI_TIMEOUT_MS));

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_got_ip);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI("ThingsCloud", "WiFi connected");
        factory_config::time::get_time()->sync_ntp();
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
        /* 超时后完全停止 WiFi 驱动，下次 start() 可重新初始化连接（如手机热点后开）*/
        wifi->stop();
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

    /* 没有有效的 SSID 就不连 */
    if (m_ssid.empty() || m_ssid[0] == '\0')
    {
        ESP_LOGW("ThingsCloud", "WiFi SSID is empty, skip connect");
        return;
    }

    m_wifi_connecting = true;

    /* 在独立任务中连接 WiFi，不阻塞调用者 */
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

/** WiFi 意外断连时（非用户手动 stop()），更新状态和 UI 图标 */
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

    /* 取消正在等待的连接任务 */
    if (m_wifi_evt)
    {
        xEventGroupSetBits(m_wifi_evt, WIFI_CANCEL_BIT);
    }

    auto* wifi = factory_config::network::get_wifi();
    wifi->disconnect();

    /* 完全停止 WiFi 驱动 */
    wifi->stop();

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

    /* stop_mqtt 会 destroy MQTT client，所以重启前需要重新 init */
    auto* mqtt = factory_config::network::get_mqtt();
    if (!mqtt->is_inited())
    {
        MqttConfig cfg = {};
        cfg.broker_uri = m_mqtt_uri.c_str();
        cfg.client_id  = m_mqtt_client_id.c_str();
        cfg.username   = m_mqtt_user.c_str();
        cfg.password   = m_mqtt_pass.c_str();
        cfg.keepalive_seconds = m_keepalive;
        mqtt->set_config(cfg);
        mqtt->init(mqtt_event_cb, this);
    }

    xTaskCreatePinnedToCore(mqtt_task_entry, "mqtt_io", 6*1024, this, 5, &m_mqtt_task, 1);
    ESP_LOGI("ThingsCloud", "MQTT task started");
}

void ThingsCloudApp::stop_mqtt()
{
    if (m_mqtt_task)
    {
        auto* mqtt = factory_config::network::get_mqtt();
        mqtt->disconnect();
        vTaskDelete(m_mqtt_task);
        m_mqtt_task = nullptr;
        ESP_LOGI("ThingsCloud", "MQTT task stopped");
    }
}

void ThingsCloudApp::report(float temp, uint32_t humi)
{
    auto* mqtt = factory_config::network::get_mqtt();
    if (!mqtt->is_connected()) return;
    if (temp == m_last_temp && humi == m_last_humi) return;

    char msg[128];
    snprintf(msg, sizeof(msg), "{\"temperature\":%.1f,\"humidity\":%lu}", temp, humi);
    mqtt->publish("attributes", (const uint8_t*)msg, strlen(msg), 0);
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

