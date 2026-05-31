#pragma once
#include "app_base.hpp"
#include <cstdlib>

/* ── 播放状态机 ── */
enum class MusicState {
    kIdle,      /* 无歌曲加载 */
    kLoading,   /* 文件 I/O / 解码准备中 */
    kPlaying,   /* 正常播放中 */
    kPaused,    /* 暂停 */
    kError,     /* 文件缺失 / 解码失败 */
};

/* 歌曲信息 */
struct SongInfo 
{
    const char* title;
    const char* artist;
    int duration_sec;
};

/* Layer 2: MusicApp — 播放器 UI 布局 + 焦点环 + 可重载行为钩子 */
class MusicApp : public AppBase 
{
public:
    void show() override;
    void hide() override;
    void on_destroy() override;
    lv_obj_t* screen() const override { return m_screen; }
    const char* app_name() const override { return "音乐"; }
    const lv_image_dsc_t* app_icon() const override;

protected:
    /* ——— 可重载的歌曲数据钩子 ——— */
    virtual int get_song_count() { return 0; }
    virtual const SongInfo* get_song(int idx) { return nullptr; }
    virtual void on_song_select(int idx) {}
    virtual void on_play_pause() {}
    virtual void on_next() {}
    virtual void on_prev() {}
    virtual void on_seek(int pct) {}

    /* ——— 状态机 ——— */
    void set_state(MusicState new_state);
    MusicState get_state() const { return m_state; }
    bool is_playing() const { return m_state == MusicState::kPlaying; }
    static const char* state_str(MusicState s);

    /* ——— UI 构建 ——— */
    void build_ui();
    void focus_update();
    void update_display();
    void handle_key(uint32_t key);

    /* ——— LVGL 定时器回调 ——— */
    friend void music_nav_timer_cb(lv_timer_t* t);
    static void progress_timer_cb(lv_timer_t* t);


    lv_obj_t* m_screen        = nullptr;
    lv_obj_t* m_record        = nullptr;
    lv_obj_t* m_title         = nullptr;
    lv_obj_t* m_artist        = nullptr;
    lv_obj_t* m_prog_bar      = nullptr;
    lv_obj_t* m_prog_knob     = nullptr;
    lv_obj_t* m_time_l        = nullptr;
    lv_obj_t* m_time_r        = nullptr;
    lv_obj_t* m_vol_bar       = nullptr;
    lv_obj_t* m_mode_dd       = nullptr;
    lv_obj_t* m_mode_val      = nullptr;
    lv_obj_t* m_play_btn      = nullptr;
    lv_obj_t* m_play_mode_lbl = nullptr;

    lv_timer_t* m_prog_timer  = nullptr;
    lv_timer_t* m_nav_timer   = nullptr;   /* GPIO 轮询定时器 — 绕过 group 吞噬 */

    /* GPIO 轮询状态 */
    bool       m_nav_held      = false;
    uint32_t   m_nav_press_tick = 0;
    uint32_t   m_nav_last_tick = 0;

    int m_focus      = 0;
    bool m_adjust    = false;
    int m_cur_song   = 0;
    int m_progress   = 35;
    int m_elapsed    = 0;
    int m_volume     = 8;
    int m_audio_mode = 0;
    int m_play_mode  = 0;
    MusicState   m_state      = MusicState::kIdle;
    bool         m_playing    = false;

    enum { FOCUS_SONG, FOCUS_PROGRESS, FOCUS_PLAY_MODE, FOCUS_PLAY_CTRL, FOCUS_AUDIO_MODE, FOCUS_COUNT };

    static constexpr int RECORD_SZ = 84;
    static constexpr int RECORD_Y  = 4;
    static constexpr int TITLE_Y   = 88;
    static constexpr int ARTIST_Y  = 106;
    static constexpr int MODE_Y    = 124;
    static constexpr int PROG_Y    = 160;
    static constexpr int VOL_Y     = 186;
    static constexpr int CTRL_Y    = 208;
};

/* Layer 3: MusicImpl — 歌曲库 + 播放逻辑 */
class MusicImpl : public MusicApp {
protected:
    int get_song_count() override;
    const SongInfo* get_song(int idx) override;
    void on_song_select(int idx) override;
    void on_play_pause() override;
    void on_next() override;
    void on_prev() override;
    void on_seek(int pct) override;

    friend class SongListPage;
};

