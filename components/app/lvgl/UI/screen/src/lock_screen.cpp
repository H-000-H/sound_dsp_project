#include "lock_screen.hpp"
#include "theme.hpp"
#include "gear_page.hpp"
#include "nav/inc/card_menu.hpp"

#include <ctime>

extern "C"
{
    extern const lv_image_dsc_t homelander;
    LV_FONT_DECLARE(lv_font_montserrat_36);
}

static bool s_unlocking = false;
static lv_obj_t* s_screen      = nullptr;
static lv_obj_t* s_key_handler = nullptr;

static void unlock_to_card_menu()
{
    if (s_unlocking) return;
    s_unlocking = true;
    card_menu_show();
}

static void unlock_event_cb(lv_event_t* event)
{
    if (lv_event_get_code(event) != LV_EVENT_KEY) return;
    unlock_to_card_menu();
}

static void update_time_label(lv_timer_t* t)
{
    lv_obj_t* lb = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
    if (!lb || !lv_obj_is_valid(lb))
    {
        lv_timer_delete(t); return;
    }
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    lv_label_set_text_fmt(lb, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
}

void lock_screen_show(void)
{
    s_unlocking = false;

    if (s_screen)
    {
        lv_screen_load(s_screen);
        lv_group_t* g = lv_group_get_default();
        if (g)
        {
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, s_key_handler);
            lv_group_focus_obj(s_key_handler);
        }
        return;
    }

    /* ——— 首次创建 ——— */
    s_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(th_bg()), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);

    s_key_handler = lv_obj_create(s_screen);
    lv_obj_set_size(s_key_handler, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_key_handler, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_key_handler, 0, 0);
    lv_obj_set_style_pad_all(s_key_handler, 0, 0);
    lv_obj_remove_flag(s_key_handler, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_key_handler, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    /* 全屏壁纸 */
    lv_obj_t* image = lv_image_create(s_screen);
    lv_image_set_src(image, &homelander);
    lv_obj_set_pos(image, 0, 0);

    /* 时间 — Apple Watch 风格 */
    lv_obj_t* lock_time = lv_label_create(s_screen);
    lv_obj_set_style_text_color(lock_time, lv_color_hex(th_text()), 0);
    lv_obj_set_style_text_font(lock_time, &lv_font_montserrat_36, 0);
    lv_obj_align(lock_time, LV_ALIGN_TOP_MID, 0, 30);

    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    lv_label_set_text_fmt(lock_time, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);

    lv_timer_create(update_time_label, 1000, lock_time);

    lv_screen_load(s_screen);

    lv_group_t* g = lv_group_get_default();
    if (!g)
    {
        g = lv_group_create();
        lv_group_set_default(g);
    }
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, s_key_handler);
    lv_group_focus_obj(s_key_handler);
    lv_obj_add_event_cb(s_key_handler, unlock_event_cb, LV_EVENT_KEY, nullptr);
}
