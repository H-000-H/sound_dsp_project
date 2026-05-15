#include "serial_app.hpp"
#include "theme.hpp"
#include "ui/app/serial/serial.hpp"
#include "ui/nav/inc/card_menu.hpp"
#include "button.hpp"

extern "C" {
    LV_FONT_DECLARE(lv_font_custom_16);
    LV_FONT_DECLARE(lv_font_montserrat_14);
}

#define GPIO_NEXT  CONFIG_LVGL_KEY_NEXT_GPIO
#define GPIO_PREV  CONFIG_LVGL_KEY_PREV_GPIO
#define GPIO_ENTER CONFIG_LVGL_KEY_ENTER_GPIO
#define GPIO_ESC   CONFIG_LVGL_KEY_ESC_GPIO

/* 波特率表 */
const int BAUD_RATES[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
const int BAUD_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

const char* TERM_MODES[] = {"终端", "HEX"};
const int TERM_MODE_COUNT = 2;

/* ==================================================================== */
/*  UI 构建                                                             */
/* ==================================================================== */
void SerialApp::build_ui()
{
    m_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(m_screen, lv_color_hex(th_bg()), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);

    /* ——— 标题栏 (0~44) ——— */
    lv_obj_t* hdr = lv_obj_create(m_screen);
    lv_obj_set_size(hdr, lv_pct(100), 44);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(th_card()), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(th_border()), 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, "串口调试");
    lv_obj_set_style_text_color(title, lv_color_hex(th_text()), 0);
    lv_obj_set_style_text_font(title, &lv_font_custom_16, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

    /* 连接状态文字 + 圆点 (标题右侧) */
    m_conn_dot = lv_label_create(hdr);
    lv_label_set_text(m_conn_dot, "未连接");
    lv_obj_set_style_text_font(m_conn_dot, &lv_font_custom_16, 0);
    lv_obj_set_style_text_color(m_conn_dot, lv_color_hex(th_border_dim()), 0);
    lv_obj_align(m_conn_dot, LV_ALIGN_RIGHT_MID, -55, 0);

    /* ——— 终端输出区 (y=48, h=154) ——— */
    m_output = lv_spangroup_create(m_screen);
    lv_obj_set_size(m_output, 224, 154);
    lv_obj_set_pos(m_output, 8, 48);
    lv_obj_set_style_bg_color(m_output, lv_color_hex(th_term_bg()), 0);
    lv_obj_set_style_bg_opa(m_output, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_output, 1, 0);
    lv_obj_set_style_border_color(m_output, lv_color_hex(th_border()), 0);
    lv_obj_set_style_radius(m_output, 6, 0);
    lv_spangroup_set_align(m_output, LV_TEXT_ALIGN_LEFT);
    lv_spangroup_set_overflow(m_output, LV_SPAN_OVERFLOW_CLIP);
    lv_spangroup_set_indent(m_output, 0);
    lv_obj_remove_flag(m_output, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(m_output, &lv_font_custom_16, 0);

    /* ——— 底部状态栏 (206~240, 34px) ——— */
    lv_obj_t* sta = lv_obj_create(m_screen);
    lv_obj_set_size(sta, lv_pct(100), 34);
    lv_obj_set_pos(sta, 0, 206);
    lv_obj_set_style_bg_color(sta, lv_color_hex(th_card()), 0);
    lv_obj_set_style_bg_opa(sta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sta, 1, 0);
    lv_obj_set_style_border_color(sta, lv_color_hex(th_border()), 0);
    lv_obj_remove_flag(sta, LV_OBJ_FLAG_SCROLLABLE);

    /* 波特率 (左) */
    m_baud_val = lv_label_create(sta);
    lv_label_set_text_fmt(m_baud_val, "%d", BAUD_RATES[m_baud_idx]);
    lv_obj_set_style_text_font(m_baud_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_baud_val, lv_color_hex(th_text_dim()), 0);
    lv_obj_align(m_baud_val, LV_ALIGN_LEFT_MID, 10, 0);

    /* 终端模式 (波特率右侧) */
    m_term_val = lv_label_create(sta);
    lv_label_set_text_fmt(m_term_val, "%s", TERM_MODES[m_term_mode]);
    lv_obj_set_style_text_font(m_term_val, &lv_font_custom_16, 0);
    lv_obj_set_style_text_color(m_term_val, lv_color_hex(th_text_dim()), 0);
    lv_obj_align(m_term_val, LV_ALIGN_LEFT_MID, 72, 0);

    /* 查看历史 — 文字+透明容器 */
    {
        lv_obj_t* cont = lv_obj_create(sta);
        lv_obj_set_size(cont, 44, 22);
        lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -54, 0);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 1, 0);
        lv_obj_set_style_border_color(cont, lv_color_hex(th_border()), 0);
        lv_obj_set_style_radius(cont, 4, 0);
        lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

        m_view_lbl = lv_label_create(cont);
        lv_label_set_text(m_view_lbl, "查看");
        lv_obj_set_style_text_font(m_view_lbl, &lv_font_custom_16, 0);
        lv_obj_set_style_text_color(m_view_lbl, lv_color_hex(th_text_dim()), 0);
        lv_obj_center(m_view_lbl);
    }

    /* 清屏 — 文字+透明容器 */
    {
        lv_obj_t* cont2 = lv_obj_create(sta);
        lv_obj_set_size(cont2, 44, 22);
        lv_obj_align(cont2, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_set_style_bg_opa(cont2, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont2, 1, 0);
        lv_obj_set_style_border_color(cont2, lv_color_hex(th_border()), 0);
        lv_obj_set_style_radius(cont2, 4, 0);
        lv_obj_remove_flag(cont2, LV_OBJ_FLAG_SCROLLABLE);

        m_clear_lbl = lv_label_create(cont2);
        lv_label_set_text(m_clear_lbl, "清屏");
        lv_obj_set_style_text_font(m_clear_lbl, &lv_font_custom_16, 0);
        lv_obj_set_style_text_color(m_clear_lbl, lv_color_hex(th_text_dim()), 0);
        lv_obj_center(m_clear_lbl);
    }

    /* ——— 屏幕自身接收 LV_EVENT_KEY ——— */
    lv_obj_add_event_cb(m_screen, [](lv_event_t* e)
    {
        auto* app = static_cast<SerialApp*>(lv_event_get_user_data(e));
        if (lv_event_get_code(e) == LV_EVENT_KEY)
        {
            uint32_t key = lv_event_get_key(e);
            if (key == LV_KEY_ENTER || key == LV_KEY_ESC) app->handle_key(key);
        }
    }, LV_EVENT_KEY, this);
}

/* ==================================================================== */
/*  焦点环                                                             */
/* ==================================================================== */
void SerialApp::focus_update()
{
    if (!m_screen) return;

    auto reset_text = [](lv_obj_t* obj, uint32_t color)
    {
        if (obj) lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    };
    auto reset_border = [](lv_obj_t* obj, uint32_t bw, uint32_t color)
    {
        if (!obj) return;
        lv_obj_t* parent = lv_obj_get_parent(obj);
        if (parent)
        {
            lv_obj_set_style_border_width(parent, bw, 0);
            lv_obj_set_style_border_color(parent, lv_color_hex(color), 0);
        }
    };

    reset_text(m_baud_val, th_text_dim());
    reset_text(m_term_val, th_text_dim());
    reset_text(m_view_lbl, th_text_dim());
    reset_text(m_clear_lbl, th_text_dim());
    reset_border(m_view_lbl, 1, th_border());
    reset_border(m_clear_lbl, 1, th_border());

    lv_color_t accent = m_adjust ? lv_color_hex(0xFFFFFF) : lv_color_hex(th_accent());

    switch (m_focus)
    {
    case FOCUS_BAUD:
        lv_obj_set_style_text_color(m_baud_val, accent, 0);
        break;
    case FOCUS_TERM_MODE:
        if (m_term_val) lv_obj_set_style_text_color(m_term_val, accent, 0);
        break;
    case FOCUS_VIEW:
        if (m_view_lbl)
        {
            lv_obj_set_style_text_color(m_view_lbl, accent, 0);
            lv_obj_t* p = lv_obj_get_parent(m_view_lbl);
            if (p)
            {
                lv_obj_set_style_border_width(p, 2, 0);
                lv_obj_set_style_border_color(p, accent, 0);
            }
        }
        break;
    case FOCUS_CLEAR:
        if (m_clear_lbl)
        {
            lv_obj_set_style_text_color(m_clear_lbl, accent, 0);
            lv_obj_t* p = lv_obj_get_parent(m_clear_lbl);
            if (p)
            {
                lv_obj_set_style_border_width(p, 2, 0);
                lv_obj_set_style_border_color(p, accent, 0);
            }
        }
        break;
    }
}

/* ==================================================================== */
/*  显示更新                                                           */
/* ==================================================================== */
void SerialApp::update_display()
{
    if (m_baud_val)
        lv_label_set_text_fmt(m_baud_val, "%d", BAUD_RATES[m_baud_idx]);
    if (m_term_val)
        lv_label_set_text_fmt(m_term_val, "%s", TERM_MODES[m_term_mode]);
    if (m_view_lbl)
    {
        if (m_paused)
            lv_label_set_text_fmt(m_view_lbl, "←%d→", m_view_page);
        else
            lv_label_set_text(m_view_lbl, "查看");
    }
    if (m_conn_dot)
    {
        lv_label_set_text(m_conn_dot, m_connected ? "已连接" : "未连接");
        lv_obj_set_style_text_color(m_conn_dot,
            m_connected ? lv_color_hex(0x34C759) : lv_color_hex(CLR_RED), 0);
    }
}

/* ==================================================================== */
/*  按键处理                                                           */
/* ==================================================================== */
void SerialApp::handle_key(uint32_t key)
{
    if (key == LV_KEY_ESC)
    {
        if (m_adjust)
        {
            m_adjust = false;
            if (m_paused)
            {
                m_paused = false;
                m_view_page = 0;
                s_scroll_offset = 0;
                on_toggle_pause();
            }
            focus_update();
        }
        else
        {
            lv_async_call([](void*) { card_menu_show(); }, nullptr);
        }
        return;
    }

    if (m_adjust)
    {
        if (key == LV_KEY_ENTER)
        {
            if (m_focus == FOCUS_CLEAR)
            {
                on_clear(); update_display();
                m_adjust = false; focus_update();
            } else if (m_focus == FOCUS_VIEW)
            {
                if (m_paused)
                {
                    m_paused = false;
                    m_view_page = 0;
                    s_scroll_offset = 0;
                    on_toggle_pause();
                    update_display();
                }
                m_adjust = false; focus_update();
            } else {
                m_adjust = false; focus_update();
            }
            return;
        }

        switch (m_focus)
        {
        case FOCUS_BAUD:
            if (key == LV_KEY_NEXT)
            {
                m_baud_idx = (m_baud_idx + 1) % BAUD_COUNT; on_baud_change(m_baud_idx); update_display();
            }
            else if (key == LV_KEY_PREV)
            {
                m_baud_idx = (m_baud_idx + BAUD_COUNT - 1) % BAUD_COUNT; on_baud_change(m_baud_idx); update_display();
            }
            break;
        case FOCUS_TERM_MODE:
            if (key == LV_KEY_NEXT)
            {
                m_term_mode = (m_term_mode + 1) % TERM_MODE_COUNT; on_term_mode_change(m_term_mode); update_display();
            }
            else if (key == LV_KEY_PREV)
            {
                m_term_mode = (m_term_mode + TERM_MODE_COUNT - 1) % TERM_MODE_COUNT; on_term_mode_change(m_term_mode); update_display();
            }
            break;
        case FOCUS_VIEW:
            if (key == LV_KEY_NEXT || key == LV_KEY_PREV)
            {
                if (!m_paused)
                {
                    m_paused = true;
                    s_scroll_offset = 0;
                    m_view_page = 0;
                }
                if (key == LV_KEY_NEXT) on_view_down();
                else on_view_up();
                update_display();
            }
            break;
        case FOCUS_CLEAR:
            if (key == LV_KEY_NEXT || key == LV_KEY_PREV)
            {
                on_clear(); update_display();
            }
            break;
        }
    } else {
        if (key == LV_KEY_ENTER)
        {
            m_adjust = true; focus_update();
        }
        else if (key == LV_KEY_NEXT)
        {
            m_focus = (m_focus + 1) % FOCUS_COUNT; focus_update();
        }
        else if (key == LV_KEY_PREV)
        {
            m_focus = (m_focus + FOCUS_COUNT - 1) % FOCUS_COUNT; focus_update();
        }
    }
}

/* ==================================================================== */
/*  GPIO 轮询                                                          */
/* ==================================================================== */
void SerialApp::nav_timer_cb(lv_timer_t* t)
{
    auto* app = static_cast<SerialApp*>(lv_timer_get_user_data(t));
    if (!app || lv_screen_active() != app->screen()) return;

    static int s_last_gpio = -1;
    int gpio = Button::get_instance().get_pressed_gpio();
    bool is_nav = (gpio == GPIO_NEXT || gpio == GPIO_PREV);
    if (is_nav && gpio != s_last_gpio)
    {
        app->handle_key(gpio == GPIO_NEXT ? LV_KEY_NEXT : LV_KEY_PREV);
    }
    s_last_gpio = is_nav ? gpio : -1;
}

/* ==================================================================== */
/*  生命周期                                                           */
/* ==================================================================== */
const lv_image_dsc_t* SerialApp::app_icon() const
{
    extern const lv_image_dsc_t icon_serial_debug;
    return &icon_serial_debug;
}

void SerialApp::show()
{
    if (!m_screen)
    {
        build_ui();
        update_display();
    }

    lv_screen_load(m_screen);

    lv_group_t* g = lv_group_get_default();
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, m_screen);
    lv_group_focus_obj(m_screen);

    if (!m_nav_timer)
        m_nav_timer = lv_timer_create(nav_timer_cb, 80, this);
    if (!m_refr_timer)
        m_refr_timer = lv_timer_create(refr_timer_cb, 200, this);

    extern SerialImpl g_serial_impl;
    g_serial_impl.start_serial();

    focus_update();
}

void SerialApp::hide()
{
    if (m_nav_timer)
    {
        lv_timer_delete(m_nav_timer); m_nav_timer = nullptr;
    }
    if (m_refr_timer)
    {
        lv_timer_delete(m_refr_timer); m_refr_timer = nullptr;
    }
    /* 离开页面时不关闭串口，避免 USB 断开时崩溃 */
    m_paused = false;
    m_view_page = 0;
    s_scroll_offset = 0;
}