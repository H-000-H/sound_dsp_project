#pragma once
#include "app_base.hpp"

/* Layer 2: SettingsApp — UI 布局 + 按键导航 + 可重载行为钩子 */
class SettingsApp : public AppBase {
public:
    static constexpr int ROW_COUNT = 6;
    void show() override;
    void hide() override;
    lv_obj_t* screen() const override { return m_screen; }
    const char* app_name() const override { return "设置"; }
    const lv_image_dsc_t* app_icon() const override;

    /* ——— LVGL 事件回调（静态成员，可访问 protected 成员） ——— */
    static void row_focused_cb(lv_event_t* e);//更新焦点高亮
    static void settings_key_cb(lv_event_t* e);//按键处理
    static void settings_click_cb(lv_event_t* e);//事件处理

    /* ——— defer 用公开转发 ——— */
    void do_enter();
    void do_esc();

protected:
    /* ——— 子类可重载的硬件操作钩子 ——— */
    virtual void on_wifi_toggle(bool on) {}
    virtual void on_bt_toggle(bool on) {}
    virtual void on_mqtt_toggle(bool on) {}
    virtual void on_brightness(int val) {}
    virtual void on_low_power(bool on) {}

    /* ——— UI 构建（show 内部调用） ——— */
    void build_ui();

    lv_obj_t* m_screen    = nullptr;
    lv_obj_t* m_rows[ROW_COUNT]   = {};
    lv_obj_t* m_togs[4]   = {};
    lv_obj_t* m_bar       = nullptr;
    lv_obj_t* m_bright_lbl = nullptr;
    int       m_focus     = 0;
    int       m_initial_focus = 0;  /* show() 用，非 0 时首次聚焦到指定行 */
    uint32_t  m_last_tick = 0;
    bool      m_block_first = false;

    /* 状态 */
    bool m_wifi_on    = true;
    bool m_bt_on      = false;
    bool m_mqtt_on    = false;
    bool m_low_pwr_on = false;
    bool m_theme_dark = true;
    int  m_bright_val = 80;

    static constexpr uint32_t ICON_COLORS[ROW_COUNT] = {0x007AFF, 0x007AFF, 0xFF3B30, 0xFF9500, 0x34C759, 0x007AFF};
};

/* Layer 3: SettingsImpl — 实际硬件控制 */
class SettingsImpl : public SettingsApp 
{
protected:
    void on_wifi_toggle(bool on) override;
    void on_bt_toggle(bool on) override;
    void on_mqtt_toggle(bool on) override;
    void on_brightness(int val) override;
    void on_low_power(bool on) override;
};
