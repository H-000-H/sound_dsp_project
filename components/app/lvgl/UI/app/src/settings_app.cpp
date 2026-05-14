#include "settings_app.hpp"
#include "lvgl_main.hpp"

extern "C" {
    LV_FONT_DECLARE(lv_font_montserrat_14);
    LV_FONT_DECLARE(lv_font_montserrat_20);
    LV_FONT_DECLARE(lv_font_custom_16);
}

const lv_image_dsc_t* SettingsApp::app_icon() const
{
    extern const lv_image_dsc_t icon_settings;
    return &icon_settings;
}

/*====================================================================*/
/*  工具函数                                                          */
/*====================================================================*/
static lv_obj_t* make_round_icon(lv_obj_t* parent, const char* sym, uint32_t color)
{
    lv_obj_t* circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 32, 32);
    lv_obj_set_style_radius(circle, 16, 0);
    lv_obj_set_style_bg_color(circle, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(circle, 0, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(circle, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* icon = lv_label_create(circle);
    lv_label_set_text(icon, sym);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_center(icon);
    return circle;
}

static lv_obj_t* make_row(lv_obj_t* parent)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), 52);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    return row;
}

static void row_label(lv_obj_t* row, const char* text)
{
    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_custom_16, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 52, 0);
}

static lv_obj_t* create_toggle(lv_obj_t* parent)
{
    lv_obj_t* tog = lv_obj_create(parent);
    lv_obj_set_size(tog, 48, 28);
    lv_obj_set_style_radius(tog, 14, 0);
    lv_obj_set_style_border_width(tog, 0, 0);
    lv_obj_set_style_pad_all(tog, 0, 0);
    lv_obj_remove_flag(tog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(tog, LV_ALIGN_RIGHT_MID, -12, 0);
    return tog;
}

static void update_toggle(lv_obj_t* tog, bool on)
{
    if (!tog) return;
    if (on) {
        lv_obj_set_style_bg_color(tog, lv_color_hex(0x34C759), 0);
        lv_obj_set_style_bg_opa(tog, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(tog, lv_color_hex(0xE5E5EA), 0);
        lv_obj_set_style_bg_opa(tog, LV_OPA_COVER, 0);
    }
    lv_obj_t* knob = lv_obj_get_child(tog, 0);
    if (knob) lv_obj_del(knob);
    knob = lv_obj_create(tog);
    lv_obj_set_size(knob, 24, 24);
    lv_obj_set_style_radius(knob, 12, 0);
    lv_obj_set_style_bg_color(knob, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(knob, 0, 0);
    lv_obj_set_style_shadow_width(knob, 2, 0);
    lv_obj_set_style_shadow_color(knob, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(knob, LV_OPA_20, 0);
    lv_obj_align(knob, on ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, on ? -2 : 2, 0);
}

static lv_obj_t* create_brightness_bar(lv_obj_t* parent, int val, lv_obj_t** out_bar, lv_obj_t** out_lbl)
{
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 110, 28);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_t* bar = lv_bar_create(cont);
    lv_obj_set_size(bar, 80, 8);
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text_fmt(lbl, "%d%%", val);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, 0, 0);

    *out_bar = bar;
    *out_lbl = lbl;
    return cont;
}

/*====================================================================*/
/*  焦点 + 按键                                                        */
/*====================================================================*/
void SettingsApp::row_focused_cb(lv_event_t* e)
{
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_target(e));
    for (int i = 0; i < 5; i++) {
        if (self->m_rows[i] == row) {
            int prev = self->m_focus;
            self->m_focus = i;
            if (prev >= 0 && prev < 5 && self->m_rows[prev])
                lv_obj_set_style_bg_color(self->m_rows[prev], lv_color_hex(0xFFFFFF), 0);
            if (self->m_rows[i])
                lv_obj_set_style_bg_color(self->m_rows[i], lv_color_hex(0xE5E5EA), 0);
            lv_obj_scroll_to_view(self->m_rows[self->m_focus], LV_ANIM_ON);
            break;
        }
    }
}

void SettingsApp::settings_key_cb(lv_event_t* e)
{
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ENTER) self->do_enter();
        else if (key == LV_KEY_ESC) self->do_esc();
    }
}

void SettingsApp::settings_click_cb(lv_event_t* e)
{
    auto* self = static_cast<SettingsApp*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED) self->do_enter();
}

/*====================================================================*/
/*  动作                                                              */
/*====================================================================*/
void SettingsApp::do_enter()
{
    if (m_block_first) { m_block_first = false; return; }

    if (m_focus == 0 || m_focus == 1 || m_focus == 2 || m_focus == 4) {
        uint32_t now = lv_tick_get();
        if (lv_tick_elaps(m_last_tick) < 300) return;
        m_last_tick = now;

        if (m_focus == 0) {
            m_wifi_on = !m_wifi_on;
            update_toggle(m_togs[0], m_wifi_on);
            lvgl_defer([](void* arg) {
                auto* s = static_cast<SettingsApp*>(arg);
                s->on_wifi_toggle(s->m_wifi_on);
            }, this);
        } else if (m_focus == 1) {
            m_bt_on = !m_bt_on;
            update_toggle(m_togs[1], m_bt_on);
            on_bt_toggle(m_bt_on);
        } else if (m_focus == 2) {
            m_mqtt_on = !m_mqtt_on;
            update_toggle(m_togs[2], m_mqtt_on);
            lvgl_defer([](void* arg) {
                auto* s = static_cast<SettingsApp*>(arg);
                s->on_mqtt_toggle(s->m_mqtt_on);
            }, this);
        } else {
            m_low_pwr_on = !m_low_pwr_on;
            update_toggle(m_togs[3], m_low_pwr_on);
            on_low_power(m_low_pwr_on);
        }
    } else if (m_focus == 3) {
        m_bright_val += 5;
        if (m_bright_val > 100) m_bright_val = 100;
        lv_bar_set_value(m_bar, m_bright_val, LV_ANIM_ON);
        lv_label_set_text_fmt(m_bright_lbl, "%d%%", m_bright_val);
        on_brightness(m_bright_val);
    }
}

void SettingsApp::do_esc()
{
    if (m_focus == 3) {
        m_bright_val -= 5;
        if (m_bright_val < 0) m_bright_val = 0;
        lv_bar_set_value(m_bar, m_bright_val, LV_ANIM_ON);
        lv_label_set_text_fmt(m_bright_lbl, "%d%%", m_bright_val);
        on_brightness(m_bright_val);
    } else {
        extern void card_menu_show(void);
        lv_async_call([](void*) { card_menu_show(); }, nullptr);
    }
}

/*====================================================================*/
/*  构建 UI                                                           */
/*====================================================================*/
void SettingsApp::build_ui()
{
    m_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(m_screen, lv_color_hex(0xF2F2F7), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(m_screen);
    lv_label_set_text(title, "设置");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_custom_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 12);

    lv_obj_t* cont = lv_obj_create(m_screen);
    lv_obj_set_size(cont, lv_pct(100), 196);
    lv_obj_set_pos(cont, 0, 44);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 12, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 8, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    static const char* labels[] = {"WiFi", "蓝牙", "MQTT", "亮度", "低功耗"};
    static const char* syms[]   = {LV_SYMBOL_WIFI, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_CLOSE, LV_SYMBOL_IMAGE, LV_SYMBOL_BATTERY_FULL};
    bool* states[] = {&m_wifi_on, &m_bt_on, &m_mqtt_on, nullptr, &m_low_pwr_on};

    for (int i = 0; i < 5; i++) {
        m_rows[i] = make_row(cont);
        make_round_icon(m_rows[i], syms[i], ICON_COLORS[i]);
        row_label(m_rows[i], labels[i]);

        if (i == 3) {
            create_brightness_bar(m_rows[i], m_bright_val, &m_bar, &m_bright_lbl);
        } else {
            int tog_idx = (i < 3) ? i : 3; /* WiFi→0, BT→1, MQTT→2, 低功耗→3 */
            m_togs[tog_idx] = create_toggle(m_rows[i]);
            update_toggle(m_togs[tog_idx], *states[i]);
        }

        lv_obj_add_event_cb(m_rows[i], row_focused_cb, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(m_rows[i], settings_key_cb, LV_EVENT_KEY, this);
        lv_obj_add_event_cb(m_rows[i], settings_click_cb, LV_EVENT_SHORT_CLICKED, this);
    }
}

/*====================================================================*/
/*  生命周期                                                          */
/*====================================================================*/
void SettingsApp::show()
{
    m_focus = 0;
    m_last_tick = 0;
    m_block_first = true;

    if (m_screen) {
        lv_obj_del(m_screen);
        m_screen = nullptr;
        for (auto& r : m_rows) r = nullptr;
        m_togs[0] = m_togs[1] = m_togs[2] = m_togs[3] = nullptr;
        m_bar = m_bright_lbl = nullptr;
    }

    build_ui();

    lv_group_t* g = lv_group_get_default();
    lv_group_remove_all_objs(g);
    for (int i = 0; i < 5; i++)
        lv_group_add_obj(g, m_rows[i]);
    lv_group_focus_obj(m_rows[0]);

    /* 初始高亮第一行 */
    lv_obj_set_style_bg_color(m_rows[0], lv_color_hex(0xE5E5EA), 0);

    lv_screen_load(m_screen);
}

void SettingsApp::hide()
{
    /* 设置页不销毁，下次 show 时重建 */
}
