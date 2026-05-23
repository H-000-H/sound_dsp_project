#ifndef CAPABILITY_AUDIO_ENGINE_H
#define CAPABILITY_AUDIO_ENGINE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音频能力 — 组合功放控制 + MP3 解码 + DSP EQ
 *
 * 位于 service 和 driver/algorithm 之间:
 *   AudioService → AudioEngine → max98357a_driver + MP3 + EQ
 */
typedef struct audio_engine audio_engine_t;

struct audio_engine {
    int (*init)(audio_engine_t* eng);
    int (*play)(audio_engine_t* eng, const uint8_t* data, uint32_t len);
    int (*set_volume)(audio_engine_t* eng, float vol);
    int (*set_enable)(audio_engine_t* eng, int enable);
    void (*deinit)(audio_engine_t* eng);
    void* _impl;
};

void audio_engine_init_struct(audio_engine_t* eng);

#ifdef __cplusplus
}
#endif

#endif /* CAPABILITY_AUDIO_ENGINE_H */
