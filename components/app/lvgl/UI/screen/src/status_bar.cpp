#include "status_bar.hpp"
#include "button.hpp"
#include "theme.hpp"
#include "ui/app/inc/serial_app.hpp"

extern "C" { LV_FONT_DECLARE(lv_font_montserrat_20); }

/*====================================================================*/
/*  WiFi 图标                                                         */
/*====================================================================*/
static struct
{
    lv_obj_t* label;
    bool       connected;
} s_wifi = {nullptr, false};

void ui_set_wifi_state(bool connected) { s_wifi.connected = connected; }

static void wifi_init(void)
{
    s_wifi.label = lv_label_create(lv_layer_top());
    lv_label_set_text(s_wifi.label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi.label, lv_color_hex(th_text()), 0);
    lv_obj_set_style_text_font(s_wifi.label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_wifi.label, LV_ALIGN_TOP_RIGHT, -38, 10);
    lv_obj_add_flag(s_wifi.label, LV_OBJ_FLAG_HIDDEN);
}

/*====================================================================*/
/*  蓝牙图标                                                          */
/*====================================================================*/
static struct 
{
    lv_obj_t* label;
    bool       connected;
} s_bt = {nullptr, false};

void ui_set_bt_state(bool connected) { s_bt.connected = connected; }

static void bt_init(void)
{
    s_bt.label = lv_label_create(lv_layer_top());
    lv_label_set_text(s_bt.label, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(s_bt.label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_bt.label, lv_color_hex(th_text()), 0);
    lv_obj_align(s_bt.label, LV_ALIGN_TOP_RIGHT, -66, 10);
    lv_obj_add_flag(s_bt.label, LV_OBJ_FLAG_HIDDEN);
}

/*====================================================================*/
/*  电量图标                                                          */
/*====================================================================*/
static struct {
    lv_obj_t* label;
    int8_t     level;
} s_batt = {nullptr, -1};

void ui_set_battery_level(int8_t percent)
{
    if (percent > 100) percent = 100;
    if (percent < 0)   percent = 0;
    s_batt.level = percent;
}

static void battery_init(void)
{
    s_batt.label = lv_label_create(lv_layer_top());
    lv_label_set_text(s_batt.label, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(s_batt.label, lv_color_hex(th_text()), 0);
    lv_obj_set_style_text_font(s_batt.label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_batt.label, LV_ALIGN_TOP_RIGHT, -10, 10);
}

/*====================================================================*/
/*  音量 — 白色竖条，长按 NEXT/PREV >300ms 调节                     */
/*====================================================================*/
static struct 
{
    lv_obj_t*  cont;
    lv_obj_t*  bar;
    uint32_t   last_tick;
    int16_t    value;
} s_vol = {nullptr, nullptr, 0, 0};

int ui_get_volume(void) { return s_vol.value; }

static void vol_timer_cb(lv_timer_t* t)
{
    int gpio = Button::get_instance().get_pressed_gpio();
    bool serial_active = serial_debug_screen_is_active();

    static int      s_last_gpio   = -1;
    static uint32_t s_press_tick  = 0;

    if (!serial_active)
    {
        if (gpio == CONFIG_LVGL_KEY_NEXT_GPIO ||
            gpio == CONFIG_LVGL_KEY_PREV_GPIO)
            {
            if (gpio != s_last_gpio)
            {
                s_last_gpio  = gpio;
                s_press_tick = lv_tick_get();
            }
        } 
        else 
        {
            s_last_gpio = -1;
        }

        bool changed = false;
        if (s_last_gpio >= 0 && lv_tick_elaps(s_press_tick) > 300)
        {
            if (s_last_gpio == CONFIG_LVGL_KEY_NEXT_GPIO)
            {
                s_vol.value += 5;
                if (s_vol.value > 100) s_vol.value = 100;
                changed = true;
            } 
            else if (s_last_gpio == CONFIG_LVGL_KEY_PREV_GPIO)
            {
                s_vol.value -= 5;
                if (s_vol.value < 0) s_vol.value = 0;
                changed = true;
            }
        }

        if (changed)
        {
            lv_slider_set_value(s_vol.bar, s_vol.value, LV_ANIM_ON);
            lv_obj_remove_flag(s_vol.cont, LV_OBJ_FLAG_HIDDEN);
            s_vol.last_tick = lv_tick_get();
        }

        if (!lv_obj_has_flag(s_vol.cont, LV_OBJ_FLAG_HIDDEN) &&
            lv_tick_elaps(s_vol.last_tick) > 2000)
        {
            lv_obj_add_flag(s_vol.cont, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* WiFi 图标（串口界面强制隐藏） */
    if (s_wifi.label)
    {
        if (s_wifi.connected && !serial_active)
            lv_obj_remove_flag(s_wifi.label, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_wifi.label, LV_OBJ_FLAG_HIDDEN);
    }

    /* 蓝牙图标 */
    if (s_bt.label && lv_obj_is_valid(s_bt.label))
    {
        if (s_bt.connected && !serial_active)
            lv_obj_remove_flag(s_bt.label, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_bt.label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void vol_slider_init(void)
{
    s_vol.value = 50;

    s_vol.cont = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_vol.cont, 48, 200);
    lv_obj_align(s_vol.cont, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_opa(s_vol.cont, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(s_vol.cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(s_vol.cont, 0, 0);
    lv_obj_set_style_radius(s_vol.cont, 24, 0);
    lv_obj_remove_flag(s_vol.cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_vol.cont, LV_OBJ_FLAG_HIDDEN);

    s_vol.bar = lv_slider_create(s_vol.cont);
    lv_obj_set_size(s_vol.bar, 12, 170);
    lv_obj_center(s_vol.bar);
    lv_slider_set_range(s_vol.bar, 0, 100);
    lv_slider_set_value(s_vol.bar, s_vol.value, LV_ANIM_OFF);
    lv_obj_remove_flag(s_vol.bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_vol.bar, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_vol.bar, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(s_vol.bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_vol.bar, LV_OPA_80, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_vol.bar, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_vol.bar, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_vol.bar, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_vol.bar, 0, 0);

    lv_timer_create(vol_timer_cb, 100, NULL);
}

/*====================================================================*/
/*  公开入口                                                          */
/*====================================================================*/
void status_bar_init(void)
{
    vol_slider_init();
    wifi_init();
    bt_init();
    battery_init();
}
