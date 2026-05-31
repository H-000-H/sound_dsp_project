#include "settings_app.hpp"
#include "display.h"
#include "device.h"
#include "thingscloud_app.hpp"
#include "mqtt_client.hpp"
#include "ui/screen/inc/status_bar.hpp"

/* 全局实例（供 card_menu 引用）*/
SettingsImpl g_settings_impl;

/* 注册 WiFi 状态回调 (app 注入 service, 避免反向依赖) */
static bool s_wifi_cb_registered = []() {
    ThingsCloudApp::get_instance().set_wifi_state_cb(ui_set_wifi_state);
    return true;
}();

void SettingsImpl::on_wifi_toggle(bool on)
{
#if CONFIG_ENABLE_DEVICE_HAL_WIFI
    if (on)
        ThingsCloudApp::get_instance().start();
    else
        ThingsCloudApp::get_instance().stop();
#else
    (void)on;
#endif
}

void SettingsImpl::on_bt_toggle(bool on)
{
    (void)on;
    /* 预留：蓝牙硬件控制 */
}

void SettingsImpl::on_mqtt_toggle(bool on)
{
    auto& tc = ThingsCloudApp::get_instance();
    tc.set_mqtt_auto(on);
    if (on) tc.start_mqtt();
    else    tc.stop_mqtt();
}

void SettingsImpl::on_brightness(int val)
{
    device_t* lcd = device_find("lcd0");
    if (lcd && device_get_status(lcd) == DEVICE_STATUS_RUNNING) {
        uint8_t brightness = (uint8_t)(val * 255 / 100);
        display_set_backlight(lcd, brightness);
    }
}

void SettingsImpl::on_low_power(bool on)
{
    (void)on;
    /* 预留：低功耗模式 */
}
