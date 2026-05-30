#include "capability/audio_engine.hpp"

#include "device.h"
#include "max98357a_driver.h"
#include "media/mp3.hpp"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "audio_engine";

typedef struct 
{
    device_t* amp_dev;
} audio_engine_impl_t;

static int eng_init(audio_engine_t* eng)
{
    if (!eng || !eng->_impl) return -1;
    audio_engine_impl_t* impl = (audio_engine_impl_t*)eng->_impl;

    impl->amp_dev = device_find("speaker_amp0");
    if (!impl->amp_dev)
    {
        ESP_LOGE(kTag, "speaker_amp0 not found");
        return -1;
    }
    if (device_open(impl->amp_dev, NULL) != 0)
    {
        ESP_LOGE(kTag, "amp init failed");
        return -1;
    }
    MP3::getinstance().init();
    ESP_LOGI(kTag, "audio engine ready");
    return 0;
}

static int eng_play(audio_engine_t* eng, const uint8_t* data, uint32_t len)
{
    (void)eng;
    if (!data || !len) return -1;
    MP3::getinstance().play((uint8_t*)data, len);
    return 0;
}

static int eng_set_volume(audio_engine_t* eng, float vol)
{
    (void)eng;
    MP3::getinstance().MP3_volume(vol);
    return 0;
}

static int eng_set_enable(audio_engine_t* eng, int enable)
{
    if (!eng || !eng->_impl) return -1;
    audio_engine_impl_t* impl = (audio_engine_impl_t*)eng->_impl;
    return device_ioctl(impl->amp_dev, MAX98357A_CMD_SET_ENABLE, &enable, sizeof(enable));
}

static void eng_deinit(audio_engine_t* eng)
{
    if (!eng || !eng->_impl) return;
    audio_engine_impl_t* impl = (audio_engine_impl_t*)eng->_impl;
    if (impl->amp_dev) { int v = 0; device_ioctl(impl->amp_dev, MAX98357A_CMD_SET_ENABLE, &v, sizeof(v)); }
    free(impl);
    eng->_impl = NULL;
}

void audio_engine_init_struct(audio_engine_t* eng)
{
    if (!eng) return;
    eng->init = eng_init;
    eng->play = eng_play;
    eng->set_volume = eng_set_volume;
    eng->set_enable = eng_set_enable;
    eng->deinit = eng_deinit;
    eng->_impl = calloc(1, sizeof(audio_engine_impl_t));
}
