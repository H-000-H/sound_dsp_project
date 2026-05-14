#ifndef __BSP_MQTT_H__
#define __BSP_MQTT_H__
#include "config.hpp"
#if CONFIG_ENABLE_BSP_MQTT
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct
{
    char broker_uri[128];
    char client_id[32];
    char username[32];
    char password[32];
    uint16_t keepalive_seconds;
}bsp_mqtt_config_t;

typedef enum
{
    BSP_MQTT_EVENT_CONNECTED=0,
    BSP_MQTT_EVENT_DISCONNECTED,
    BSP_MQTT_EVENT_SUBSCRIBED,
    BSP_MQTT_EVENT_UNSUBSCRIBED,
    BSP_MQTT_EVENT_PUBLISHED,
    BSP_MQTT_EVENT_RXDATA,
    BSP_MQTT_EVENT_ERROR,
}bsp_mqtt_event_t;

/**
 * @param user_ctx 回调时透传给上层的用户上下文
 * @details MQTT 事件回调函数类型
 */
typedef void (*bsp_mqtt_event_callback_t)(bsp_mqtt_event_t event,const char* topic,const uint8_t*data,uint16_t len,void*user_ctx);

typedef struct
{
    bsp_mqtt_config_t cfg;
    void* client;
    bool is_connected;
    bsp_mqtt_event_callback_t cb;  
    void* user_ctx;
}bsp_mqtt_handle_t;
void bsp_mqtt_init(bsp_mqtt_handle_t*handle,bsp_mqtt_event_callback_t call_back,void* user_ctx);
void bsp_mqtt_connect(bsp_mqtt_handle_t*handle);
void bsp_mqtt_disconnect(bsp_mqtt_handle_t*handle);
/**
 * @param qos MQTT 服务质量等级
 */
int bsp_mqtt_publish(bsp_mqtt_handle_t*handle,const char*topic,const uint8_t*data,uint16_t len,uint8_t qos);
int bsp_mqtt_subscribe(bsp_mqtt_handle_t*handle,const char*topic,uint8_t qos);
int bsp_mqtt_unsubscribe(bsp_mqtt_handle_t*handle,const char* topic);
bool bsp_mqtt_is_connected(bsp_mqtt_handle_t* handle);
#ifdef __cplusplus
}   
#endif
#endif
#endif
