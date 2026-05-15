#include "button.hpp"
#include "theme.hpp"
#include "song_list.hpp"
#include "ui/nav/inc/card_menu.hpp"

extern "C" {
    LV_FONT_DECLARE(lv_font_custom_16);
}

#define GPIO_NEXT  CONFIG_LVGL_KEY_NEXT_GPIO
#define GPIO_PREV  CONFIG_LVGL_KEY_PREV_GPIO
#define GPIO_ENTER CONFIG_LVGL_KEY_ENTER_GPIO
#define GPIO_ESC   CONFIG_LVGL_KEY_ESC_GPIO

/* 全局实例 + 包装函数 */
SongListPage g_song_list;

void song_list_page_show(MusicImpl* app) { g_song_list.show(app); }
bool song_list_is_active(void) { return g_song_list.is_active(); }

/* ==================================================================== */
/*  UI 构建                                                             */
/* ==================================================================== */
void SongListPage::build_ui()
{
    if (m_screen) return;
    m_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(m_screen, lv_color_hex(th_bg()), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(m_screen);
    lv_label_set_text(title, "歌曲列表");
    lv_obj_set_style_text_color(title, lv_color_hex(th_text()), 0);
    lv_obj_set_style_text_font(title, &lv_font_custom_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    m_list = lv_obj_create(m_screen);
    lv_obj_set_size(m_list, lv_pct(100), 200);
    lv_obj_set_pos(m_list, 0, 36);
    lv_obj_set_style_bg_opa(m_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_list, 0, 0);
    lv_obj_set_flex_flow(m_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_list, 0, 0);
    lv_obj_set_style_pad_row(m_list, 2, 0);
    lv_obj_remove_flag(m_list, LV_OBJ_FLAG_SCROLLABLE);

    m_dots = lv_obj_create(m_screen);
    lv_obj_set_size(m_dots, lv_pct(100), 16);
    lv_obj_align(m_dots, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(m_dots, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_dots, 0, 0);
    lv_obj_set_flex_flow(m_dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(m_dots, 8, 0);
    lv_obj_remove_flag(m_dots, LV_OBJ_FLAG_SCROLLABLE);
}

/* ==================================================================== */
/*  列表更新                                                             */
/* ==================================================================== */
void SongListPage::update_list()
{
    if (!m_list) return;
    lv_obj_clean(m_list);

    int total = m_app ? m_app->get_song_count() : 0;
    int start = m_page * PER_PAGE;
    int end = start + PER_PAGE;
    if (end > total) end = total;

    for (int i = start; i < end; i++)
    {
        auto* info = m_app ? m_app->get_song(i) : nullptr;
        if (!info) break;

        lv_obj_t* row = lv_obj_create(m_list);
        lv_obj_set_size(row, lv_pct(100), 36);
        lv_obj_set_style_bg_color(row, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(th_border()), 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* idx_lbl = lv_label_create(row);
        lv_label_set_text_fmt(idx_lbl, "%d", i + 1);
        lv_obj_set_style_text_color(idx_lbl, lv_color_hex(th_text_dim()), 0);
        lv_obj_set_style_text_font(idx_lbl, &lv_font_custom_16, 0);
        lv_obj_align(idx_lbl, LV_ALIGN_LEFT_MID, 12, 0);

        lv_obj_t* name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, info->title);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(th_text()), 0);
        lv_obj_set_style_text_font(name_lbl, &lv_font_custom_16, 0);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 36, 0);

        if (i - start == m_focus)
        {
            lv_obj_set_style_bg_color(row, lv_color_hex(th_card_hl()), 0);
            lv_obj_set_style_border_color(row, lv_color_hex(th_accent()), 0);
            lv_obj_set_style_border_width(row, 2, 0);
        }
    }

    if (m_dots)
    {
        lv_obj_clean(m_dots);
        int pages = (total + PER_PAGE - 1) / PER_PAGE;
        if (pages < 1) pages = 1;
        for (int p = 0; p < pages; p++)
        {
            lv_obj_t* dot = lv_obj_create(m_dots);
            lv_obj_set_size(dot, 8, 8);
            lv_obj_set_style_radius(dot, 4, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_set_style_bg_color(dot,
                p == m_page ? lv_color_hex(th_accent()) : lv_color_hex(th_border_dim()), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        }
    }
}

/* ==================================================================== */
/*  GPIO 轮询 — 处理全部 4 个按键                                        */
/* ==================================================================== */
void SongListPage::song_list_nav_timer_cb(lv_timer_t* t)
{
    auto* self = static_cast<SongListPage*>(lv_timer_get_user_data(t));
    if (!self || !self->m_screen) return;
    if (lv_screen_active() != self->m_screen) return;

    int gpio = Button::get_instance().get_pressed_gpio();
    if (gpio < 0)
    {
        self->m_pending_enter = false;
        self->m_nav_held = false; return;
    }

    uint32_t key = 0;
    if (gpio == GPIO_NEXT) key = LV_KEY_NEXT;
    else if (gpio == GPIO_PREV) key = LV_KEY_PREV;
    else if (gpio == GPIO_ENTER) key = LV_KEY_ENTER;
    else if (gpio == GPIO_ESC) key = LV_KEY_ESC;
    if (!key) return;

    /* 防抖 */
    static uint32_t s_last_key = 0;
    static uint32_t s_last_tick = 0;
    uint32_t now = lv_tick_get();
    if (key == s_last_key && (now - s_last_tick) < 80) return;
    s_last_key = key;
    s_last_tick = now;
    self->m_nav_held = true;

    if (key == LV_KEY_ESC)
    {
        auto* app = self->m_app;
        self->hide();
        lv_async_call([](void* arg) { static_cast<MusicImpl*>(arg)->show(); }, app);
        return;
    }

    if (key == LV_KEY_ENTER)
    {
        if (self->m_pending_enter) return;  /* 等待 ENTER 松开再接受选歌 */
        int idx = self->m_page * PER_PAGE + self->m_focus;
        if (self->m_app && idx < self->m_app->get_song_count())
        {
            self->select(idx);
        }
        return;
    }

    if (key == LV_KEY_NEXT)
    {
        int on_page = self->m_app ? self->m_app->get_song_count() - self->m_page * PER_PAGE : 0;
        if (on_page > PER_PAGE) on_page = PER_PAGE;
        self->m_focus++;
        if (self->m_focus >= on_page)
        {
            self->m_focus = 0;
            int total = self->m_app ? self->m_app->get_song_count() : 0;
            int pages = (total + PER_PAGE - 1) / PER_PAGE;
            if (pages < 1) pages = 1;
            self->m_page = (self->m_page + 1) % pages;
        }
        self->update_list();
    }

    if (key == LV_KEY_PREV)
    {
        self->m_focus--;
        if (self->m_focus < 0)
        {
            int total = self->m_app ? self->m_app->get_song_count() : 0;
            int pages = (total + PER_PAGE - 1) / PER_PAGE;
            if (pages < 1) pages = 1;
            self->m_page = (self->m_page + pages - 1) % pages;
            int on_page = total - self->m_page * PER_PAGE;
            if (on_page > PER_PAGE) on_page = PER_PAGE;
            self->m_focus = on_page - 1;
        }
        self->update_list();
    }
}

/* ==================================================================== */
/*  选择歌曲                                                             */
/* ==================================================================== */
void SongListPage::select(int idx)
{
    if (!m_app) return;
    auto* app = m_app;
    app->on_song_select(idx);
    app->m_playing = true;
    this->hide();
    lv_async_call([](void* arg) { static_cast<MusicImpl*>(arg)->show(); }, app);
}

/* ==================================================================== */
/*  生命周期                                                             */
/* ==================================================================== */
void SongListPage::show(MusicImpl* app)
{
    m_app = app;
    m_page = 0;
    m_focus = 0;

    build_ui();
    update_list();

    lv_group_t* g = lv_group_get_default();
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, m_screen);
    lv_group_focus_obj(m_screen);

    if (!m_nav_timer)
        m_nav_timer = lv_timer_create(song_list_nav_timer_cb, 50, this);

    m_pending_enter = true;
    m_nav_held = false;
    lv_screen_load(m_screen);
}

void SongListPage::hide()
{
    if (m_nav_timer)
    {
        lv_timer_delete(m_nav_timer); m_nav_timer = nullptr;
    }
    if (m_screen)
    {
        lv_obj_del(m_screen);
        m_screen = nullptr;
    }
}
