#include "settings_app.hpp"
#include "device.h"
#include "hal_pwm.h"
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
    static hal_pwm_channel_t s_pwm;
    static bool s_pwm_inited = false;

    if (!s_pwm_inited)
    {
        hal_pwm_init_struct(&s_pwm);

        int pin = 15; /* fallback */
        device_t* dev = device_find("lcd0");
        if (dev) device_get_prop_int(dev, "backlight", &pin);

        s_pwm.init(&s_pwm, pin, 5000, 10);
        s_pwm_inited = true;
    }

    uint32_t duty = (val * 1023) / 100; /* 10-bit resolution */
    s_pwm.set_duty(&s_pwm, duty);
}

void SettingsImpl::on_low_power(bool on)
{
    (void)on;
    /* 预留：低功耗模式 */
}
