#include "settings_app.hpp"
#include "pwm_controller.hpp"
#include "thingscloud_app.hpp"
#include "mqtt_client.hpp"

/* 全局实例（供 card_menu 引用）*/
SettingsImpl g_settings_impl;

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
    static bool pwm_inited = false;
    if (!pwm_inited) {
        PwmOutputChannel::get_instance().init();
        pwm_inited = true;
    }
    uint32_t duty = (val * 255) / 100;
    PwmOutputChannel::get_instance().set_duty(duty);
}

void SettingsImpl::on_low_power(bool on)
{
    (void)on;
    /* 预留：低功耗模式 */
}
