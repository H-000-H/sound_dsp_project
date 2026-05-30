#include "media/mp3.hpp"

#include "device.h"
#include "system_log.hpp"

#include <cstring>

static constexpr const char* kTag = "MP3";

static device_t* s_i2s_dev = nullptr;

void MP3::init()
{
    if (s_i2s_dev) return;

    s_i2s_dev = device_find("i2s_audio0");
    if (!s_i2s_dev)
    {
        SYS_LOGE(kTag, "i2s_audio0 not found in device tree");
        return;
    }
    SYS_LOGI(kTag, "I2S device found");
}

void MP3::play(uint8_t* src_data, uint32_t len)
{
    HMP3Decoder dec = MP3InitDecoder();
    uint8_t* read_ptr = src_data;
    int bytes_left = static_cast<int>(len);

    while (bytes_left > 0)
    {
        int sync = MP3FindSyncWord(read_ptr, bytes_left);
        if (sync < 0) break;
        read_ptr += sync;
        bytes_left -= sync;

        int ret = MP3Decode(dec, &read_ptr, &bytes_left, (short*)m_buffer, 0);
        if (ret != 0) continue;

        MP3GetLastFrameInfo(dec, &info);
        size_t size = info.outputSamps * sizeof(short) * 2;
        size_t written = 0;

        uint32_t samples = size / sizeof(int16_t);
        for (uint32_t i = 0; i < samples; i++)
        {
            q31_buffer[i] = (int32_t)m_buffer[i] << 15;
        }
        filter->vptr->process(&filter->config, q31_buffer, q31_buffer, samples);
        for (uint32_t i = 0; i < samples; i++)
        {
            m_buffer[i] = (int16_t)(q31_buffer[i] >> 15);
        }
        if (volume->flag_change == true) volume->process_audio_volume(m_buffer, samples);

        if (s_i2s_dev)
        {
            device_write(s_i2s_dev, m_buffer, size);
        }
    }
    MP3FreeDecoder(dec);
}

void MP3::MP3_filter(filter_classfiy type, uint32_t f0, uint32_t Q, int32_t gain_db)
{
    filter->vptr->setset_filter(type, f0, Q, gain_db);
}

void MP3::MP3_volume(float v)
{
    volume->set_audio_volume(v);
}
