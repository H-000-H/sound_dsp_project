#include "main.hpp"
#include "system_runtime.hpp"
#include "ui_service.hpp"

#include "audio_service.hpp"
#include "cloud_service.hpp"
#include "event_bus.hpp"
#include "key_input.hpp"
#include "mqtt_client.hpp"
#include "tcp_client.hpp"
#include "thingscloud_app.hpp"

void lvgl_main(void);

EXTERN_C void app_main(void)
{
    /*
     * 提前"触摸"所有 Meyer's Singleton, 在调度器/中断启动前
     * 强制完成 __cxa_guard_acquire 隐藏互斥锁,
     * 杜绝 ISR 内首次调用 getInstance 的 ABI 死锁.
     */
    (void)EventBus::getInstance();
    (void)KeyInput::getInstance();
    (void)MqttClient::get_instance();
    (void)TcpClient::get_instance();
    (void)AudioService::getInstance();
    (void)UiService::getInstance();
    (void)CloudService::getInstance();
    (void)ThingsCloudApp::get_instance();

    UiService::set_entry(lvgl_main);
    SystemRuntime::getInstance().start();
}