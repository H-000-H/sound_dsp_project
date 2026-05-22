#include "music_app.hpp"
#include "ui/nav/inc/card_menu.hpp"
#include "key_input.hpp"
#include "ui/app/music/song_list.hpp"
#include "theme.hpp"

extern "C"
{
    LV_FONT_DECLARE(lv_font_montserrat_14);
    LV_FONT_DECLARE(lv_font_montserrat_20);
    LV_FONT_DECLARE(lv_font_custom_16);
}

/*====================================================================*/
/*  歌曲播放状态 — 由 impl 提供数据                                   */
/*====================================================================*/
static const char* s_mode_names[] = {"原声", "标准", "加强", "自定义"};

/* 按键 GPIO 从 DeviceTree 读取 */
#define GPIO_NEXT  KeyInput::getGpioNext()
#define GPIO_PREV  KeyInput::getGpioPrev()
#define GPIO_ENTER KeyInput::getGpioEnter()
#define GPIO_ESC   KeyInput::getGpioEsc()

/*====================================================================*/
/*  UI 构建                                                           */
/*====================================================================*/
void MusicApp::build_ui()
{
    if (m_screen) return;
    m_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(m_screen, lv_color_hex(th_bg()), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);

    /* ——— 黑胶唱片 ——— */
    {
        lv_obj_t* base = lv_obj_create(m_screen);
        lv_obj_set_size(base, RECORD_SZ, RECORD_SZ);
        lv_obj_set_pos(base, (240 - RECORD_SZ) / 2, RECORD_Y);
        lv_obj_set_style_bg_color(base, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(base, 1, 0);
        lv_obj_set_style_border_color(base, lv_color_hex(th_border()), 0);
        lv_obj_set_style_radius(base, RECORD_SZ / 4, 0);
        lv_obj_remove_flag(base, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_shadow_width(base, 12, 0);
        lv_obj_set_style_shadow_color(base, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(base, LV_OPA_50, 0);
        m_record = base;

        lv_obj_t* disc = lv_obj_create(base);
        lv_obj_set_size(disc, RECORD_SZ - 16, RECORD_SZ - 16);
        lv_obj_center(disc);
        lv_obj_set_style_bg_color(disc, lv_color_hex(0x0E0F14), 0);
        lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(disc, 0, 0);
        lv_obj_set_style_radius(disc, (RECORD_SZ - 16) / 2, 0);
        lv_obj_remove_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
        // ... 环形装饰 ...
        lv_obj_t* ring1 = lv_obj_create(disc);
        lv_obj_set_size(ring1, RECORD_SZ - 24, RECORD_SZ - 24);
        lv_obj_center(ring1);
        lv_obj_set_style_border_width(ring1, 1, 0);
        lv_obj_set_style_border_color(ring1, lv_color_hex(th_border_dim()), 0);
        lv_obj_set_style_bg_opa(ring1, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ring1, (RECORD_SZ - 24) / 2, 0);
        lv_obj_remove_flag(ring1, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ring2 = lv_obj_create(disc);
        lv_obj_set_size(ring2, RECORD_SZ / 2, RECORD_SZ / 2);
        lv_obj_center(ring2);
        lv_obj_set_style_border_width(ring2, 1, 0);
        lv_obj_set_style_border_color(ring2, lv_color_hex(th_border_dim()), 0);
        lv_obj_set_style_bg_opa(ring2, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ring2, RECORD_SZ / 4, 0);
        lv_obj_remove_flag(ring2, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* dot = lv_obj_create(disc);
        lv_obj_set_size(dot, 22, 22);
        lv_obj_center(dot);
        lv_obj_set_style_bg_color(dot, lv_color_hex(th_accent()), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_radius(dot, 11, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* hole = lv_obj_create(dot);
        lv_obj_set_size(hole, 5, 5);
        lv_obj_center(hole);
        lv_obj_set_style_bg_color(hole, lv_color_hex(0x0E0F14), 0);
        lv_obj_set_style_bg_opa(hole, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hole, 0, 0);
        lv_obj_set_style_radius(hole, 3, 0);
        lv_obj_remove_flag(hole, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* ——— 歌曲信息 ——— */
    {
        m_title = lv_label_create(m_screen);
        lv_obj_set_style_text_color(m_title, lv_color_hex(th_text()), 0);
        lv_obj_set_style_text_font(m_title, &lv_font_custom_16, 0);
        lv_obj_align(m_title, LV_ALIGN_TOP_MID, 0, TITLE_Y);

        m_artist = lv_label_create(m_screen);
        lv_obj_set_style_text_color(m_artist, lv_color_hex(th_text_sec()), 0);
        lv_obj_set_style_text_font(m_artist, &lv_font_custom_16, 0);
        lv_obj_align(m_artist, LV_ALIGN_TOP_MID, 0, ARTIST_Y);
    }

    /* ——— 音效下拉 ——— */
    {
        m_mode_dd = lv_obj_create(m_screen);
        lv_obj_set_size(m_mode_dd, 100, 28);
        lv_obj_set_pos(m_mode_dd, 4, MODE_Y + 1);
        lv_obj_set_style_radius(m_mode_dd, 14, 0);
        lv_obj_set_style_pad_all(m_mode_dd, 0, 0);
        lv_obj_remove_flag(m_mode_dd, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(m_mode_dd, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(m_mode_dd, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(m_mode_dd, 1, 0);
        lv_obj_set_style_border_color(m_mode_dd, lv_color_hex(th_border_dim()), 0);

        lv_obj_t* lbl = lv_label_create(m_mode_dd);
        lv_label_set_text(lbl, "音色");
        lv_obj_set_style_text_font(lbl, &lv_font_custom_16, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(th_text_sec()), 0);
        lv_obj_set_pos(lbl, 12, (28 - 16) / 2);

        m_mode_val = lv_label_create(m_mode_dd);
        lv_obj_set_style_text_font(m_mode_val, &lv_font_custom_16, 0);
        lv_obj_set_style_text_color(m_mode_val, lv_color_hex(th_text()), 0);
        lv_obj_set_pos(m_mode_val, 50, (28 - 16) / 2);
    }

    /* ——— 进度条 ——— */
    {
        int py = PROG_Y;

        lv_obj_t* track = lv_obj_create(m_screen);
        lv_obj_set_size(track, 180, 4);
        lv_obj_align(track, LV_ALIGN_TOP_MID, 0, py);
        lv_obj_set_style_bg_color(track, lv_color_hex(th_border()), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(track, 0, 0);
        lv_obj_set_style_radius(track, 2, 0);
        lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

        m_prog_bar = lv_bar_create(m_screen);
        lv_obj_set_size(m_prog_bar, 180, 4);
        lv_obj_align(m_prog_bar, LV_ALIGN_TOP_MID, 0, py);
        lv_bar_set_range(m_prog_bar, 0, 100);
        lv_obj_set_style_bg_opa(m_prog_bar, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(m_prog_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(m_prog_bar, lv_color_hex(th_accent()), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(m_prog_bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(m_prog_bar, 2, LV_PART_INDICATOR);
        lv_obj_remove_flag(m_prog_bar, LV_OBJ_FLAG_SCROLLABLE);

        m_prog_knob = lv_obj_create(m_screen);
        lv_obj_set_size(m_prog_knob, 12, 12);
        lv_obj_set_style_radius(m_prog_knob, 6, 0);
        lv_obj_set_style_bg_color(m_prog_knob, lv_color_hex(th_accent()), 0);
        lv_obj_set_style_bg_opa(m_prog_knob, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(m_prog_knob, 0, 0);
        lv_obj_remove_flag(m_prog_knob, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_shadow_width(m_prog_knob, 6, 0);
        lv_obj_set_style_shadow_color(m_prog_knob, lv_color_hex(th_accent()), 0);
        lv_obj_set_style_shadow_opa(m_prog_knob, LV_OPA_40, 0);

        m_time_l = lv_label_create(m_screen);
        lv_obj_set_style_text_color(m_time_l, lv_color_hex(th_text_dim()), 0);
        lv_obj_set_style_text_font(m_time_l, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(m_time_l, 8, py + 8);

        m_time_r = lv_label_create(m_screen);
        lv_obj_set_style_text_color(m_time_r, lv_color_hex(th_text_dim()), 0);
        lv_obj_set_style_text_font(m_time_r, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(m_time_r, 206, py + 8);
    }

    /* ——— 音量条 ——— */
    {
        m_vol_bar = lv_bar_create(m_screen);
        lv_obj_set_size(m_vol_bar, 100, 6);
        lv_obj_set_pos(m_vol_bar, 70, VOL_Y);
        lv_bar_set_range(m_vol_bar, 0, 10);
        lv_obj_set_style_bg_color(m_vol_bar, lv_color_hex(th_border()), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(m_vol_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(m_vol_bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(m_vol_bar, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(m_vol_bar, lv_color_hex(th_accent()), LV_PART_INDICATOR);
        lv_obj_set_style_radius(m_vol_bar, 3, LV_PART_INDICATOR);
        lv_obj_remove_flag(m_vol_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* vlbl = lv_label_create(m_screen);
        lv_label_set_text(vlbl, "音量");
        lv_obj_set_style_text_color(vlbl, lv_color_hex(th_text_sec()), 0);
        lv_obj_set_style_text_font(vlbl, &lv_font_custom_16, 0);
        lv_obj_set_pos(vlbl, 34, VOL_Y - 2);
    }

    /* ——— ENTER/ESC 通过 LV_EVENT_KEY 接收 ——— */
    lv_obj_add_event_cb(m_screen, [](lv_event_t* e)
    {
        auto* app = static_cast<MusicApp*>(lv_event_get_user_data(e));
        if (lv_event_get_code(e) == LV_EVENT_KEY)
        {
            uint32_t key = lv_event_get_key(e);
            /* LVGL 9.5: group 只吐出 ENTER/ESC，PREV/NEXT 被吞噬 */
            if (key == LV_KEY_ENTER || key == LV_KEY_ESC)
            {
                app->handle_key(key);
            }
        }
    }, LV_EVENT_KEY, this);

    /* ——— 播放控制 ——— */
    {
        lv_obj_t* prev = lv_obj_create(m_screen);
        lv_obj_set_size(prev, 32, 32);
        lv_obj_set_pos(prev, 62, CTRL_Y);
        lv_obj_set_style_bg_color(prev, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(prev, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(prev, 1, 0);
        lv_obj_set_style_border_color(prev, lv_color_hex(th_border()), 0);
        lv_obj_set_style_radius(prev, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(prev, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* pi = lv_label_create(prev);
        lv_label_set_text(pi, LV_SYMBOL_PREV);
        lv_obj_set_style_text_color(pi, lv_color_hex(th_text()), 0);
        lv_obj_set_style_text_font(pi, &lv_font_montserrat_20, 0);
        lv_obj_center(pi);

        m_play_btn = lv_obj_create(m_screen);
        lv_obj_set_size(m_play_btn, 32, 32);
        lv_obj_set_pos(m_play_btn, 104, CTRL_Y);
        lv_obj_set_style_bg_color(m_play_btn, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(m_play_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(m_play_btn, 2, 0);
        lv_obj_set_style_border_color(m_play_btn, lv_color_hex(th_text_dim()), 0);
        lv_obj_set_style_radius(m_play_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(m_play_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* play_ic = lv_label_create(m_play_btn);
        lv_label_set_text(play_ic, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(play_ic, lv_color_hex(0x0E0F14), 0);
        lv_obj_set_style_text_font(play_ic, &lv_font_montserrat_20, 0);
        lv_obj_center(play_ic);

        lv_obj_t* next = lv_obj_create(m_screen);
        lv_obj_set_size(next, 32, 32);
        lv_obj_set_pos(next, 146, CTRL_Y);
        lv_obj_set_style_bg_color(next, lv_color_hex(th_card()), 0);
        lv_obj_set_style_bg_opa(next, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(next, 1, 0);
        lv_obj_set_style_border_color(next, lv_color_hex(th_border()), 0);
        lv_obj_set_style_radius(next, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(next, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ni = lv_label_create(next);
        lv_label_set_text(ni, LV_SYMBOL_NEXT);
        lv_obj_set_style_text_color(ni, lv_color_hex(th_text()), 0);
        lv_obj_set_style_text_font(ni, &lv_font_montserrat_20, 0);
        lv_obj_center(ni);

        m_play_mode_lbl = lv_label_create(m_screen);
        lv_obj_set_style_text_font(m_play_mode_lbl, &lv_font_custom_16, 0);
        lv_obj_set_style_text_color(m_play_mode_lbl, lv_color_hex(th_text_sec()), 0);
        lv_obj_align(m_play_mode_lbl, LV_ALIGN_RIGHT_MID, -10, CTRL_Y + 16);
    }
}
/*====================================================================*/
/*  焦点环                                                             */
/*====================================================================*/
void MusicApp::focus_update()
{
    if (!m_screen) return;

    auto reset = [](lv_obj_t* obj, uint32_t bw, lv_color_t bc, lv_color_t bg)
    {
        if (!obj) return;
        lv_obj_set_style_border_width(obj, bw, 0);
        lv_obj_set_style_border_color(obj, bc, 0);
        lv_obj_set_style_shadow_width(obj, 0, 0);
        lv_obj_set_style_bg_color(obj, bg, 0);
        lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    };

    reset(m_record, 1, lv_color_hex(th_border()), lv_color_hex(th_card()));
    if (m_prog_knob)
    {
        lv_obj_set_style_border_width(m_prog_knob, 0, 0);
        lv_obj_set_style_shadow_width(m_prog_knob, 6, 0);
        lv_obj_set_style_shadow_color(m_prog_knob, lv_color_hex(th_accent()), 0);
        lv_obj_set_style_bg_color(m_prog_knob, lv_color_hex(th_accent()), 0);
    }
    if (m_play_btn && !m_playing)
    {
        lv_obj_set_style_border_width(m_play_btn, 2, 0);
        lv_obj_set_style_border_color(m_play_btn, lv_color_hex(th_text_dim()), 0);
        lv_obj_set_style_bg_color(m_play_btn, lv_color_hex(th_card()), 0);
    }
    reset(m_mode_dd, 1, lv_color_hex(th_border()), lv_color_hex(th_card()));
    if (m_play_mode_lbl)
        lv_obj_set_style_text_color(m_play_mode_lbl, lv_color_hex(th_text_sec()), 0);

    uint32_t bw = 3;
    lv_color_t accent = m_adjust ? lv_color_hex(0xFFFFFF) : lv_color_hex(th_accent());
    lv_color_t highlight = m_adjust ? lv_color_hex(th_border()) : lv_color_hex(th_card_hl());

    switch (m_focus)
    {
    case FOCUS_SONG:
        if (m_record)
        {
            lv_obj_set_style_border_width(m_record, bw, 0);
            lv_obj_set_style_border_color(m_record, accent, 0);
            lv_obj_set_style_shadow_width(m_record, 22, 0);
            lv_obj_set_style_shadow_color(m_record, accent, 0);
            lv_obj_set_style_shadow_opa(m_record, LV_OPA_60, 0);
            lv_obj_set_style_bg_color(m_record, highlight, 0);
        }
        break;
    case FOCUS_PROGRESS:
        if (m_prog_knob)
        {
            lv_obj_set_style_border_width(m_prog_knob, bw, 0);
            lv_obj_set_style_border_color(m_prog_knob, accent, 0);
            lv_obj_set_style_shadow_width(m_prog_knob, 18, 0);
            lv_obj_set_style_shadow_color(m_prog_knob, accent, 0);
            lv_obj_set_style_shadow_opa(m_prog_knob, LV_OPA_60, 0);
            lv_obj_set_style_bg_color(m_prog_knob, lv_color_hex(0xFFFFFF), 0);
        }
        break;
    case FOCUS_PLAY_MODE:
        if (m_play_mode_lbl)
        {
            lv_obj_set_style_text_color(m_play_mode_lbl, accent, 0);
            lv_obj_set_style_text_opa(m_play_mode_lbl, LV_OPA_COVER, 0);
        }
        break;
    case FOCUS_PLAY_CTRL:
        if (m_play_btn)
        {
            lv_obj_set_style_border_width(m_play_btn, bw, 0);
            lv_obj_set_style_border_color(m_play_btn, accent, 0);
            lv_obj_set_style_shadow_width(m_play_btn, 18, 0);
            lv_obj_set_style_shadow_color(m_play_btn, accent, 0);
            lv_obj_set_style_shadow_opa(m_play_btn, LV_OPA_60, 0);
            lv_obj_set_style_bg_color(m_play_btn, highlight, 0);
        }
        break;
    case FOCUS_AUDIO_MODE:
        if (m_mode_dd)
        {
            lv_obj_set_style_border_width(m_mode_dd, bw, 0);
            lv_obj_set_style_border_color(m_mode_dd, accent, 0);
            lv_obj_set_style_bg_color(m_mode_dd, highlight, 0);
        }
        break;
    }
}

/*====================================================================*/
/*  显示更新                                                           */
/*====================================================================*/
void MusicApp::update_display()
{
    if (!m_title || !m_artist) return;

    auto* song = get_song(m_cur_song);
    if (!song) return;

    lv_label_set_text(m_title, song->title);
    lv_label_set_text(m_artist, song->artist);

    lv_bar_set_value(m_prog_bar, m_progress, LV_ANIM_OFF);
    if (m_prog_knob)
    {
        lv_obj_set_pos(m_prog_knob, 30 + 180 * m_progress / 100, PROG_Y - 4);
    }

    if (m_elapsed > song->duration_sec) m_elapsed = song->duration_sec;
    lv_label_set_text_fmt(m_time_l, "%d:%02d", m_elapsed / 60, m_elapsed % 60);
    lv_label_set_text_fmt(m_time_r, "%d:%02d", song->duration_sec / 60, song->duration_sec % 60);

    lv_label_set_text_fmt(m_mode_val, "%s", s_mode_names[m_audio_mode]);

    static const char* pm[] = {"顺序", "单曲", "随机"};
    lv_label_set_text(m_play_mode_lbl, pm[m_play_mode]);

    if (m_play_btn)
    {
        lv_obj_t* ic = lv_obj_get_child(m_play_btn, 0);
        if (ic) lv_label_set_text(ic, m_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        if (m_playing)
        {
            lv_obj_set_style_bg_color(m_play_btn, lv_color_hex(th_accent()), 0);
            lv_obj_set_style_border_width(m_play_btn, 0, 0);
        } 
        else 
        {
            lv_obj_set_style_bg_color(m_play_btn, lv_color_hex(th_card()), 0);
            lv_obj_set_style_border_width(m_play_btn, 2, 0);
            lv_obj_set_style_border_color(m_play_btn, lv_color_hex(th_text_dim()), 0);
        }
    }
}

/*====================================================================*/
/*  按键处理                                                          */
/*====================================================================*/
void MusicApp::handle_key(uint32_t key)
{
    

    /* 防抖：80ms内相同按键忽略 */
    static uint32_t s_last_key = 0;
    static uint32_t s_last_tick = 0;
    uint32_t now = lv_tick_get();
    if (key == s_last_key && (now - s_last_tick) < 80) return;
    s_last_key = key;
    s_last_tick = now;

    if (key == LV_KEY_ESC)
    {
        if (m_adjust)
        {
            m_adjust = false; focus_update();
        }
        else
        {
            lv_async_call([](void*) { card_menu_show(); }, nullptr);
        }
        return;
    }

    if (m_adjust)
    {
        if (m_focus == FOCUS_PLAY_CTRL)
        {
            if (key == LV_KEY_ENTER) 
            { 
                m_playing = !m_playing; on_play_pause(); update_display(); 
            }
            else if (key == LV_KEY_NEXT) 
            { 
                on_next(); update_display(); 
            }
            else if (key == LV_KEY_PREV) 
            { 
                on_prev(); update_display(); 
            }
            return;
        }
        if (key == LV_KEY_ENTER)
        {
            m_adjust = false; focus_update(); return;
        }

        switch (m_focus)
        {
        case FOCUS_PROGRESS:
            if (key == LV_KEY_NEXT) 
            { 
                m_progress = m_progress + 5 > 100 ? 100 : m_progress + 5; on_seek(m_progress); update_display(); 
            }
            else if (key == LV_KEY_PREV) 
            { 
                m_progress = m_progress < 5 ? 0 : m_progress - 5; on_seek(m_progress); update_display(); 
            }
            break;
        case FOCUS_PLAY_MODE:
            if (key == LV_KEY_NEXT) 
            { 
                m_play_mode = (m_play_mode + 1) % 3; update_display(); 
            }
            else if (key == LV_KEY_PREV) 
            { 
                m_play_mode = (m_play_mode + 2) % 3; update_display(); 
            }
            break;
        case FOCUS_AUDIO_MODE:
            if (key == LV_KEY_NEXT) 
            { 
                m_audio_mode = (m_audio_mode + 1) % 4; update_display(); 
            }
            else if (key == LV_KEY_PREV) 
            { 
                m_audio_mode = (m_audio_mode + 3) % 4; update_display(); 
            }
            break;
        default: break;
        }
    } 
    else 
    {
        if (key == LV_KEY_ENTER)
        {
            if (m_focus == FOCUS_SONG)
            {
                lv_async_call([](void* arg)
                {
                    song_list_page_show(static_cast<MusicImpl*>(arg));
                }, this);
            } 
            else 
            {
                 m_adjust = true; focus_update(); 
            }
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

/*====================================================================*/
/*  GPIO 轮询定时器 — 绕过 LVGL group 吞噬 PREV/NEXT                  */
/*====================================================================*/
void music_nav_timer_cb(lv_timer_t* t)
{
    auto* app = static_cast<MusicApp*>(lv_timer_get_user_data(t));
    if (!app || !app->screen()) return;
    if (lv_screen_active() != app->screen())
    {
        app->m_nav_held = false; return;
    }
    
    /* 仅处理 NEXT/PREV 上升沿（ENTER/ESC 由 LV_EVENT_KEY 处理）*/
    static int s_last_gpio = -1;
    int gpio = KeyInput::getInstance().get_pressed_gpio();
    bool is_nav = (gpio == GPIO_NEXT || gpio == GPIO_PREV);
    
    if (is_nav && gpio != s_last_gpio)
    {
        uint32_t key = (gpio == GPIO_NEXT) ? LV_KEY_NEXT : LV_KEY_PREV;
        app->handle_key(key);
    }
    
    s_last_gpio = is_nav ? gpio : -1;
}

/*====================================================================*/
/*  进度定时器                                                        */
/*====================================================================*/
void MusicApp::progress_timer_cb(lv_timer_t* t)
{
    auto* app = static_cast<MusicApp*>(lv_timer_get_user_data(t));
    if (!app || lv_screen_active() != app->screen()) return;

    /* 同步音量 */
    extern int ui_get_volume(void);
    int new_vol = ui_get_volume() / 10;
    if (new_vol > 10) new_vol = 10;
    if (new_vol != app->m_volume)
    {
        app->m_volume = new_vol;
        if (app->m_vol_bar) lv_bar_set_value(app->m_vol_bar, new_vol, LV_ANIM_OFF);
    }

    if (!app->m_playing) return;

    app->m_elapsed++;
    auto* song = app->get_song(app->m_cur_song);
    if (!song) return;

    if (app->m_elapsed >= song->duration_sec)
    {
        app->m_elapsed = 0;
        if (app->m_play_mode == 1)
        {
            app->m_progress = 0;
            app->update_display();
            return;
        } 
        else if (app->m_play_mode == 2)
        {
            int nxt;
            int cnt = app->get_song_count();
            if (cnt <= 1)
            {
                nxt = 0;
            }
            else
            {
                do { nxt = rand() % cnt; } while (nxt == app->m_cur_song);
            }
            app->m_cur_song = nxt;
            app->on_song_select(nxt);
        } 
        else 
        {
            int cnt = app->get_song_count();
            if (cnt > 0) app->m_cur_song = (app->m_cur_song + 1) % cnt;
            app->on_song_select(app->m_cur_song);
        }
    }
    int dur = song->duration_sec;
    if (dur <= 0) dur = 1;
    app->m_progress = app->m_elapsed * 100 / dur;
    app->update_display();
}

/*====================================================================*/
/*  生命周期                                                          */
/*====================================================================*/
void MusicApp::show()
{
    if (!m_screen) 
    {
        srand(lv_tick_get());
        build_ui();
    }

    lv_screen_load(m_screen);
    lv_group_t* g = lv_group_get_default();
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, m_screen);
    lv_group_focus_obj(m_screen);

    /* 创建 GPIO 轮询定时器（替代被 group 吞噬的 LV_EVENT_KEY）*/
    if (!m_nav_timer)
        m_nav_timer = lv_timer_create(music_nav_timer_cb, 50, this);

    if (!m_prog_timer)
        m_prog_timer = lv_timer_create(progress_timer_cb, 1000, this);

    m_nav_held = false;
    focus_update();
    update_display();
}

void MusicApp::hide()
{
    if (m_prog_timer)
    {
        lv_timer_delete(m_prog_timer);
        m_prog_timer = nullptr;
    }
    if (m_nav_timer)
    {
        lv_timer_delete(m_nav_timer);  
        m_nav_timer  = nullptr;
    }
}

const lv_image_dsc_t* MusicApp::app_icon() const
{
    extern const lv_image_dsc_t icon_music;
    return &icon_music;
}



