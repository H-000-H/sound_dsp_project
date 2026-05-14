#pragma once
#include "ui/app/inc/music_app.hpp"

/* Layer 4: 选歌子页面 — 竖向列表、分页、ENTER 选歌跳回 */
class SongListPage {
public:
    void show(MusicImpl* app);
    void hide();
    lv_obj_t* screen() const { return m_screen; }
    bool is_active() const { return m_screen != nullptr && lv_screen_active() == m_screen; }

private:
    static void song_list_nav_timer_cb(lv_timer_t* t);

    void build_ui();
    void update_list();
    void select(int idx);

    MusicImpl* m_app = nullptr;
    lv_obj_t* m_screen = nullptr;
    lv_obj_t* m_list = nullptr;
    lv_obj_t* m_dots = nullptr;
    lv_timer_t* m_nav_timer = nullptr;

    bool       m_nav_held = false;
    int m_page = 0;
    int m_focus = 0;

    static constexpr int PER_PAGE = 5;

    friend void song_list_page_show(MusicImpl* app);
    friend bool song_list_is_active(void);
};

/* 全局唯⼀实例 — 在 song_list.cpp 中定义 */
extern SongListPage g_song_list;

/* 包装函数：仅声明，实现在 song_list.cpp 中 */
void song_list_page_show(MusicImpl* app);
bool song_list_is_active(void);
