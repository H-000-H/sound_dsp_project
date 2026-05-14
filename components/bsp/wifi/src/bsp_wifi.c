#include "bsp_wifi.h"
#if CONFIG_ENABLE_BSP_WIFI

static const char *TAG = "bsp_wifi";

static void bsp_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    bsp_wifi_handle_t *handle = (bsp_wifi_handle_t *)arg;
    if (!handle) return;

    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        /* 区分手动断开和连接丢失 */
        if (handle->manual_disconnect)
        {
            /* 手动断开：告诉应用层，但不自动重连 */
            handle->is_connected = false;
            if (handle->on_disconnected)
            {
                handle->on_disconnected();
            }
            return;
        }
        else
        {
            /* 连接丢失（意外断开）：自动重试 */
            ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
            esp_wifi_connect();
        }
        break;

    case WIFI_EVENT_STA_CONNECTED:
        handle->is_connected = true;
        ESP_LOGI(TAG, "WiFi connected");
        break;

    default:
        break;
    }
}

static void default_wifi_config(bsp_wifi_handle_t *handle)
{
    if (!handle) return;

    memset(handle->cfg.ssid, 0, sizeof(handle->cfg.ssid));
    memset(handle->cfg.sta_password, 0, sizeof(handle->cfg.sta_password));
    memset(handle->cfg.ap_name, 0, sizeof(handle->cfg.ap_name));
    memset(handle->cfg.ap_password, 0, sizeof(handle->cfg.ap_password));

    handle->cfg.channel = 1;
    handle->cfg.max_connection_count = 4;
    handle->cfg.wifi_mode = WIFI_MODE_APSTA;

    /* 默认 AP 配置 */
    memcpy(handle->cfg.ap_name, "esp32_ap", sizeof("esp32_ap"));
    memcpy(handle->cfg.ap_password, "123456789", sizeof("123456789"));

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
}

void bsp_wifi_init(bsp_wifi_handle_t *handle)
{
    if (!handle) return;
    handle->is_connected = false;
    handle->manual_disconnect = false;
    handle->on_disconnected = NULL;

    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 初始化 TCP/IP 和事件循环 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 默认 WiFi 配置 */
    default_wifi_config(handle);

    /* 初始化 WiFi 驱动 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(handle->cfg.wifi_mode));

    /* 注册事件处理器 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &bsp_wifi_event_handler, handle));
}

void bsp_wifi_connect(bsp_wifi_handle_t *handle)
{
    if (!handle) return;

    /* 显式发起连接 — 允许 BSP 事件处理器在连接失败时自动重试 */
    handle->manual_disconnect = false;

    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK)
    {
        /* 首次启动，BSP 事件处理器会收到 WIFI_EVENT_STA_START 并调用 esp_wifi_connect() */
        return;
    }
    if (err == ESP_ERR_INVALID_STATE)
    {
        /* WiFi 已在运行（首次启动后或重连），无需 stop/start 重启驱动。
         * 此时无线参数仍然在驱动中，直接发起连接即可。
         * BSP DISCONNECTED 事件处理器（manual_disconnect 机制）会在
         * 连接失败时自动重试，直到连上或用户手动断开。 */
        ESP_LOGI(TAG, "WiFi already started, connecting...");
        esp_wifi_connect();
        return;
    }
    ESP_LOGE(TAG, "WiFi start failed: %d", err);
}

void bsp_wifi_disconnect(bsp_wifi_handle_t *handle)
{
    if (!handle) return;

    /* 设置 manual_disconnect 标志 — BSP 事件处理器在收到 WIFI_EVENT_STA_DISCONNECTED
     * 时会检查此标志，跳过自动重连，避免手动断开后又被连回去 */
    handle->manual_disconnect = true;
    handle->is_connected = false;
    esp_wifi_disconnect();
}

void bsp_wifi_stop(bsp_wifi_handle_t *handle)
{
    if (!handle) return;
    bsp_wifi_disconnect(handle);
    esp_wifi_stop();
}

bool bsp_wifi_is_connected(bsp_wifi_handle_t *handle)
{
    return handle ? handle->is_connected : false;
}

void bsp_wifi_set_disconnected_cb(bsp_wifi_handle_t *handle, bsp_wifi_disconnected_cb_t cb)
{
    if (handle)
    {
        handle->on_disconnected = cb;
    }
}

#endif