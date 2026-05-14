#include "bsp_mqtt.h"
#if CONFIG_ENABLE_BSP_MQTT
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>
/**
 * 将 ESP-IDF 的 MQTT 事件分发给已注册的 BSP 回调函数。
 */
static void mqtt_event_handler(void*handler_args,esp_event_base_t base,int32_t event_id,void*event_data)
{
    bsp_mqtt_handle_t*handle=(bsp_mqtt_handle_t*)handler_args;
    bsp_mqtt_event_callback_t usr_cb = handle->cb;
    esp_mqtt_event_handle_t event =event_data;
    switch((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        handle->is_connected=true;
        if(usr_cb)
        {
            usr_cb(BSP_MQTT_EVENT_CONNECTED,NULL,NULL,0,handle->user_ctx);
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    {
        handle->is_connected=false;
        if(usr_cb)
        {
            usr_cb(BSP_MQTT_EVENT_DISCONNECTED,NULL,NULL,0,handle->user_ctx);
        }
        break;
    }
    case MQTT_EVENT_SUBSCRIBED:
    {
        if(usr_cb)
        {
            usr_cb(BSP_MQTT_EVENT_SUBSCRIBED,NULL,NULL,0,handle->user_ctx);
        }
        break;
    }
    case MQTT_EVENT_UNSUBSCRIBED:
    {
        if(usr_cb)
        {
            usr_cb(BSP_MQTT_EVENT_UNSUBSCRIBED,NULL,NULL,0,handle->user_ctx);
        }
        break;
    }
    case MQTT_EVENT_PUBLISHED:
    {
        if(usr_cb)
        {
            usr_cb(BSP_MQTT_EVENT_PUBLISHED,NULL,NULL,0,handle->user_ctx);
        }
        break;
    }
    case MQTT_EVENT_DATA:
    {
        if(usr_cb)
        {
            char topic_buf[256] = {0};
            int tlen = event->topic_len < sizeof(topic_buf)-1 ? event->topic_len : sizeof(topic_buf)-1;
            memcpy(topic_buf, event->topic, tlen);
            topic_buf[tlen] = '\0';
            usr_cb(BSP_MQTT_EVENT_RXDATA, topic_buf, (uint8_t*)event->data, event->data_len, handle->user_ctx);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
    {
        ESP_LOGW("BSP_MQTT","MQTT_EVENT_ERROR, type=%d", event->error_handle->error_type);
        if(event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGW("BSP_MQTT","Connection refused, code=%d", event->error_handle->connect_return_code);
        }
        if(usr_cb)
        {
            usr_cb(BSP_MQTT_EVENT_ERROR,NULL,NULL,0,handle->user_ctx);
        }
        break;
    }
    default:
    {
        break;
    }
    }
}

void bsp_mqtt_init(bsp_mqtt_handle_t*handle,bsp_mqtt_event_callback_t call_back,void* user_ctx)
{
    if(!handle)
    {
        return;
    }
    handle->cb = call_back;
    handle->user_ctx = user_ctx;
    esp_mqtt_client_config_t mqtt_cfg =
    {
        .broker.address.uri =handle->cfg.broker_uri,
        .credentials.client_id =handle->cfg.client_id,
        .credentials.username =handle->cfg.username,
        .credentials.authentication.password =handle->cfg.password,
        .session.keepalive =handle->cfg.keepalive_seconds,
    };
    handle->client=esp_mqtt_client_init(&mqtt_cfg);
    if(handle->client == NULL)
    {
        ESP_LOGE("BSP_MQTT", "esp_mqtt_client_init failed");
        return;
    }

    esp_err_t err = esp_mqtt_client_register_event((esp_mqtt_client_handle_t)handle->client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   handle);
    if(err != ESP_OK)
    {
        ESP_LOGE("BSP_MQTT", "register event failed: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy((esp_mqtt_client_handle_t)handle->client);
        handle->client = NULL;
    }
}

void bsp_mqtt_connect(bsp_mqtt_handle_t*handle)
{
    if(!handle||!handle->client)
    {
        return;
    }
    esp_mqtt_client_start((esp_mqtt_client_handle_t)handle->client);
}

void bsp_mqtt_disconnect(bsp_mqtt_handle_t*handle)
{
     if(!handle||!handle->client)
     {
         return;
     }
     esp_mqtt_client_stop((esp_mqtt_client_handle_t)handle->client);
     esp_mqtt_client_destroy((esp_mqtt_client_handle_t)handle->client);
     handle->client=NULL;
     handle->is_connected=false;
}
bool bsp_mqtt_is_connected(bsp_mqtt_handle_t* handle)
{
    return handle ? handle->is_connected : false;
}

int bsp_mqtt_publish(bsp_mqtt_handle_t*handle,const char*topic,const uint8_t*data,uint16_t len,uint8_t qos)
{
    if(!handle||!handle->client)
    {
        return -1;
    }
    return esp_mqtt_client_publish((esp_mqtt_client_handle_t)handle->client,topic,(const char*)data,len,qos,0);
}

int bsp_mqtt_subscribe(bsp_mqtt_handle_t*handle,const char*topic,uint8_t qos)
{
    if(!handle||!handle->client)
    {
        return -1;
    }
    return esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)handle->client, topic, qos);
}
int bsp_mqtt_unsubscribe(bsp_mqtt_handle_t*handle,const char* topic)
{
    if(!handle||!handle->client)
    {
        return -1;
    }
    return esp_mqtt_client_unsubscribe((esp_mqtt_client_handle_t)handle->client, topic);
}
#endif
