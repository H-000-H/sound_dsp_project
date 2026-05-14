#include "music_app.hpp"
#include "music/song_list.hpp"

MusicImpl g_music_impl;


/* 歌曲库 */
static const SongInfo s_songs[] = {
    { "爱你",           "王心凌",   210 },
    { "第一次爱的人",   "王心凌",   225 },
    { "当你",           "王心凌",   200 },
    { "睫毛弯弯",       "王心凌",   195 },
    { "Honey",          "王心凌",   190 },
    { "起风了",         "买辣椒也用券", 320 },
    { "青花瓷",         "周杰伦",   240 },
    { "告白气球",       "周杰伦",   220 },
    { "七里香",         "周杰伦",   245 },
    { "稻香",           "周杰伦",   230 },
};
static constexpr int SONG_COUNT = sizeof(s_songs) / sizeof(s_songs[0]);

int MusicImpl::get_song_count() { return SONG_COUNT; }

const SongInfo* MusicImpl::get_song(int idx)
{
    if (idx < 0 || idx >= SONG_COUNT) return nullptr;
    return &s_songs[idx];
}

void MusicImpl::on_song_select(int idx)
{
    m_cur_song = idx;
    m_elapsed = 0;
    m_progress = 0;
    update_display();
}

void MusicImpl::on_play_pause()
{
    /* 预留：实际音频播放控制 */
}

void MusicImpl::on_next()
{
    m_cur_song = (m_cur_song + 1) % SONG_COUNT;
    m_elapsed = 0;
    m_progress = 0;
    on_song_select(m_cur_song);
}

void MusicImpl::on_prev()
{
    m_cur_song = (m_cur_song + SONG_COUNT - 1) % SONG_COUNT;
    m_elapsed = 0;
    m_progress = 0;
    on_song_select(m_cur_song);
}

void MusicImpl::on_seek(int pct)
{
    m_progress = pct;
    auto* song = get_song(m_cur_song);
    if (song) m_elapsed = song->duration_sec * pct / 100;
    update_display();
}

/* ==================================================================== */

/* ==================================================================== */
