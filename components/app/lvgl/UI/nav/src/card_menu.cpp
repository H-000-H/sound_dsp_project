/* Layer 2 - 卡片轮播菜单
 * 一次展示 2 个卡片，PREV/NEXT 切换，ENTER 进入，ESC 返回锁屏
 *
 * 按键路由：
 *   PREV/NEXT — GPIO 轮询（nav_timer_cb）
 *   ENTER/ESC — LV_EVENT_KEY → key_cb()
 */
#include "card_menu.hpp"
#include "theme.hpp"
#include "ui/screen/inc/lock_screen.hpp"
#include "app/inc/app_base.hpp"
#include "app/inc/settings_app.hpp"
#include "app/inc/music_app.hpp"
#include "app/inc/serial_app.hpp"
#include "button.hpp"
#include "factory.hpp"

extern "C"
{
    LV_FONT_DECLARE(lv_font_montserrat_14);
    LV_FONT_DECLARE(lv_font_montserrat_20);
    LV_FONT_DECLARE(lv_font_custom_16);
    extern const lv_image_dsc_t icon_settings;
    extern const lv_image_dsc_t icon_music;
    extern const lv_image_dsc_t icon_serial_debug;
}

#define GPIO_NEXT  CONFIG_LVGL_KEY_NEXT_GPIO
#define GPIO_PREV  CONFIG_LVGL_KEY_PREV_GPIO

/*====================================================================*/
/*  应用条目 — 直接使用 AppBase 指针                                  */
/*====================================================================*/
struct AppEntry 
{
    const char*   name;
    const lv_image_dsc_t* img;
    AppBase*      app;
};

/* 前置声明：Impl 类实例（在对应 impl.cpp 中定义）*/
extern SettingsImpl g_settings_impl;
extern MusicImpl    g_music_impl;
extern SerialImpl   g_serial_impl;

static const AppEntry s_apps[] = 
{
    {"设置",     &icon_settings,      &g_settings_impl},
    {"音乐",     &icon_music,         &g_music_impl},
    {"串口调试", &icon_serial_debug,  &g_serial_impl},
};
static const int APP_COUNT = sizeof(s_apps) / sizeof(s_apps[0]);

/* 布局参数 */
static const int ICON_SZ    = 60;
static const int CARD_PAD   = 16;
static const int CARD_W     = ICON_SZ + CARD_PAD * 2;
static const int CARD_H     = ICON_SZ + CARD_PAD * 2 + 22;
static const int GRID_GAP   = 20;
static const int STEP       = CARD_W + GRID_GAP;
static const int VISIBLE    = 2;
static const int CONT_W     = VISIBLE * CARD_W + (VISIBLE-1) * GRID_GAP;
static const int CONT_X     = (240 - CONT_W) / 2;
static const int CONT_Y     = (240 - CARD_H) / 2 + 10;

/* 静态状态 */
static lv_obj_t*   s_screen      = nullptr;
static lv_obj_t*   s_card_row    = nullptr;
static lv_obj_t*   s_key_handler = nullptr;
static lv_obj_t*   s_time_label  = nullptr;
static lv_obj_t*   s_cards[3]    = {nullptr};
static lv_obj_t*   s_dots[3]     = {nullptr};
static int         s_page        = 0;
static bool        s_animating   = false;
static lv_timer_t* s_nav_timer   = nullptr;

static void animate_to(int new_page);

/*====================================================================*/
/*  高亮 + 圆点                                                        */
/*====================================================================*/
static void update_highlight()
{
    for (int i = 0; i < APP_COUNT; i++)
    {
        if (!s_cards[i]) continue;
        if (i == s_page)
        {
            lv_obj_set_style_border_color(s_cards[i], lv_color_hex(th_accent()), 0);
            lv_obj_set_style_border_width(s_cards[i], 3, 0);
            lv_obj_t* img = lv_obj_get_child(s_cards[i], 0);//获取卡片的图标对应的对象
            if (img) lv_obj_set_style_img_recolor_opa(img, LV_OPA_TRANSP, 0);
        } 
        else 
        {
            lv_obj_set_style_border_color(s_cards[i], lv_color_hex(th_border()), 0);
            lv_obj_set_style_border_width(s_cards[i], 1, 0);
            lv_obj_t* img = lv_obj_get_child(s_cards[i], 0);
            if (img) lv_obj_set_style_img_recolor_opa(img, LV_OPA_30, 0);
        }
    }
}

static void update_dots()
{
    for (int i = 0; i < APP_COUNT; i++)
    {
        if (!s_dots[i]) continue;
        if (i == s_page)
        {
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(th_text()), 0);
            lv_obj_set_size(s_dots[i], 8, 8);
        } 
        else 
        {
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(th_text_sec()), 0);
            lv_obj_set_size(s_dots[i], 6, 6);
        }
    }
}

/*====================================================================*/
/*  页面切换动画                                                      */
/*====================================================================*/
static void animate_to(int new_page)
{
    if (new_page < 0 || new_page >= APP_COUNT) return;
    if (new_page == s_page || s_animating) return;//相同或者正在动画
    if (!s_card_row || !lv_obj_is_valid(s_card_row)) return;// 对象有效性检查
    s_animating = true;

    int cur_x = lv_obj_get_x(s_card_row);
    /*判断是否是第一页和最后一页的切换*/
    bool wrap = (new_page == 0 && s_page == APP_COUNT - 1) || (new_page == APP_COUNT - 1 && s_page == 0);
    if (wrap)
    {/*隐藏中间卡片*/
        for (int i = 1; i < APP_COUNT - 1; i++)
            lv_obj_add_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
    }
    int tar_x;
    if (new_page == 0)//第一页左对齐
        tar_x = 0;
    else if (new_page == APP_COUNT - 1)
        tar_x = CONT_W - (new_page * STEP + CARD_W);//右对齐
    else
        tar_x = CONT_W / 2 - (new_page * STEP + CARD_W / 2);//中间页居中

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_card_row);
    lv_anim_set_exec_cb(&a, [](void* v, int32_t vv) { lv_obj_set_x((lv_obj_t*)v, vv); });//每帧回调函数
    lv_anim_set_values(&a, cur_x, tar_x);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, [](lv_anim_t*)
    {
        s_animating = false;
        for (int i = 0; i < APP_COUNT; i++)
        {
            if (s_cards[i])
                lv_obj_remove_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
        }
        update_dots();
        update_highlight();
    });
    lv_anim_start(&a);
    s_page = new_page;
}

/*====================================================================*/
/*  GPIO 轮询定时器                                                   */
/*====================================================================*/
static void nav_timer_cb(lv_timer_t* t)
{
    (void)t;
    static bool s_prev_held = false;
    if (lv_screen_active() != s_screen)
    {
        s_prev_held = false; return;
    }

    int gpio = Button::get_instance().get_pressed_gpio();
    bool now_held = (gpio == GPIO_NEXT || gpio == GPIO_PREV);

    if (now_held && !s_prev_held)
    {
        if (gpio == GPIO_NEXT)
            animate_to(s_page < APP_COUNT - 1 ? s_page + 1 : 0);
        else
            animate_to(s_page > 0 ? s_page - 1 : APP_COUNT - 1);
    }
    s_prev_held = now_held;
}

/*====================================================================*/
/*  按键回调                                                          */
/*====================================================================*/
static void key_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);

    if (key == LV_KEY_NEXT)
    {
        animate_to(s_page < APP_COUNT - 1 ? s_page + 1 : 0);
    }
    else if (key == LV_KEY_PREV)
    {
        animate_to(s_page > 0 ? s_page - 1 : APP_COUNT - 1);
    }
    else if (key == LV_KEY_ENTER)
    {
        /* 使用 AppBase 的 show() 进入应用 */
        lv_async_call([](void* arg)
        {
            auto* app = static_cast<AppBase*>(arg);
            app->show();
        }, s_apps[s_page].app);
    }
    else if (key == LV_KEY_ESC)
    {
        lock_screen_show();
    }
}

/*====================================================================*/
/*  时间标签                                                          */
/*====================================================================*/
static void time_update_cb(lv_timer_t* t)
{
    lv_obj_t* lb = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
    if (!lb || !lv_obj_is_valid(lb))
    {
        lv_timer_delete(t); return;
    }
    auto* ti = factory_config::time::get_time();
    auto info = ti->get_time();
    lv_label_set_text_fmt(lb, "%02d:%02d", info.hour, info.minute);
}

/*====================================================================*/
/*  创建 UI 元素                                                      */
/*====================================================================*/
static void create_cards()
{
    lv_obj_t* cont = lv_obj_create(s_screen);
    lv_obj_set_size(cont, CONT_W, CARD_H);
    lv_obj_set_pos(cont, CONT_X, CONT_Y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);//透明背景
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_clip_corner(cont, true, 0);//启动裁剪超出大小直接裁剪
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    s_card_row = lv_obj_create(cont);
    lv_obj_set_size(s_card_row, LV_SIZE_CONTENT, CARD_H);
    lv_obj_set_pos(s_card_row, 0, 0);
    lv_obj_set_style_bg_opa(s_card_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_card_row, 0, 0);
    lv_obj_set_style_pad_all(s_card_row, 0, 0);
    lv_obj_set_style_layout(s_card_row, LV_LAYOUT_NONE, 0);//手动控制卡片位置
    lv_obj_remove_flag(s_card_row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < APP_COUNT; i++)
    {
        lv_obj_t* card = lv_obj_create(s_card_row);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, i * STEP, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(th_border()), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        s_cards[i] = card;

        lv_obj_t* img = lv_image_create(card);
        lv_image_set_src(img, s_apps[i].img);
        lv_obj_set_size(img, ICON_SZ, ICON_SZ);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, CARD_PAD);
        lv_obj_set_style_radius(img, 14, 0);
        lv_obj_set_style_clip_corner(img, true, 0);
        lv_obj_set_style_img_recolor(img, lv_color_hex(0x000000), 0);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_30, 0);

        lv_obj_t* lbl = lv_label_create(card);
        lv_label_set_text(lbl, s_apps[i].name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(th_text_sec()), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_custom_16, 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -6);
    }
}

static void create_dots()
{
    int dot_y = CONT_Y + CARD_H + 10;
    int total_w = APP_COUNT * 8 + (APP_COUNT - 1) * 8;
    int dot_x = (240 - total_w) / 2;

    for (int i = 0; i < APP_COUNT; i++)
    {
        s_dots[i] = lv_obj_create(s_screen);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_dots[i], 0, 0);
        lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(s_dots[i], dot_x + i * 16, dot_y);

        if (i == 0)
        {
            lv_obj_set_size(s_dots[i], 8, 8);
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(th_text()), 0);
        } else {
            lv_obj_set_size(s_dots[i], 6, 6);
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(th_text_sec()), 0);
        }
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
    }
}

static void create_time_label()
{
    s_time_label = lv_label_create(s_screen);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(th_text()), 0);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_time_label, LV_ALIGN_TOP_LEFT, 10, 8);

    auto* ti = factory_config::time::get_time();
    auto info = ti->get_time();
    lv_label_set_text_fmt(s_time_label, "%02d:%02d", info.hour, info.minute);
    lv_timer_create(time_update_cb, 1000, s_time_label);
}

/*====================================================================*/
/*  显示入口                                                          */
/*====================================================================*/
void card_menu_show(void)
{
    lv_group_t* g = lv_group_get_default();

    if (s_screen)
    {
        lv_screen_load(s_screen);
        s_animating = false;
        {
            int restore_x;
            if (s_page == 0)
                restore_x = 0;//第一页左对齐
            else if (s_page == APP_COUNT - 1)
                restore_x = CONT_W - (s_page * STEP + CARD_W);//最后一页右对齐
            else
                restore_x = CONT_W / 2 - (s_page * STEP + CARD_W / 2);//居中
            lv_obj_set_x(s_card_row, restore_x);
        }
        update_highlight();
        update_dots();

        lv_group_remove_all_objs(g);
        lv_group_add_obj(g, s_key_handler);
        lv_group_focus_obj(s_key_handler);

        if (!s_nav_timer)
            s_nav_timer = lv_timer_create(nav_timer_cb, 50, nullptr);
        return;
    }

    s_page = 0;
    s_animating = false;

    s_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(th_bg()), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);

    create_time_label();
    create_cards();
    create_dots();

    s_key_handler = lv_obj_create(s_screen);
    lv_obj_set_size(s_key_handler, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_key_handler, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_key_handler, 0, 0);
    lv_obj_set_style_pad_all(s_key_handler, 0, 0);
    lv_obj_remove_flag(s_key_handler, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_key_handler, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_key_handler, key_cb, LV_EVENT_KEY, nullptr);

    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, s_key_handler);
    lv_group_focus_obj(s_key_handler);

    update_highlight();
    update_dots();

    s_nav_timer = lv_timer_create(nav_timer_cb, 50, nullptr);
    lv_screen_load(s_screen);
}
