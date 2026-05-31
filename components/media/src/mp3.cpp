#include "media/mp3.hpp"

#include "device.h"
#include "esp_heap_caps.h"
#include "system_log.hpp"
#include "task_config.h"

#include <cstring>

static constexpr const char* kTag = "MP3";

/*
 * PCM 缓冲策略 (IEC 61508 §7.4.3 音频安全):
 *   32KB 内部 SRAM → 44100Hz stereo 16-bit → 185ms 缓冲
 *   远超 Flash 擦写最坏 100ms Cache 禁用窗口.
 *   Static API 锁定 BSS 段, aligned(64) 满足 ESP32-S3 Cache Line.
 *   +1 StreamBuffer sentinel, 总尺寸向上取整至 64 倍数 (32832B).
 */
#define PCM_STREAM_SIZE       32768
#define PCM_STREAM_BUFFER_SZ  (((PCM_STREAM_SIZE + 1) + 63) & ~63U)
#define FEED_CHUNK_BYTES       1024

static device_t* s_i2s_dev = nullptr;
static StreamBufferHandle_t s_pcm_stream = nullptr;
static TaskHandle_t s_feed_task = nullptr;

static uint8_t s_pcm_buffer_storage[PCM_STREAM_BUFFER_SZ] __attribute__((aligned(64)));
static StaticStreamBuffer_t s_pcm_stream_struct;

static void i2s_feeder_task(void* arg)
{
    (void)arg;
    int16_t buf[FEED_CHUNK_BYTES / sizeof(int16_t)];

    while (true)
    {
        size_t received = xStreamBufferReceive(s_pcm_stream, buf, sizeof(buf),
                                                portMAX_DELAY);
        if (s_i2s_dev && received > 0)
        {
            device_write(s_i2s_dev, buf, received, portMAX_DELAY);
        }
    }
}

void MP3::init()
{
    if (s_i2s_dev) return;

    s_i2s_dev = device_find("i2s_audio0");
    if (!s_i2s_dev)
    {
        SYS_LOGE(kTag, "i2s_audio0 not found in device tree");
        return;
    }

    s_pcm_stream = xStreamBufferCreateStatic(
        PCM_STREAM_SIZE, 64,
        s_pcm_buffer_storage,
        &s_pcm_stream_struct);
    configASSERT(s_pcm_stream != nullptr);

    SYS_LOGI(kTag, "PCM StreamBuffer %uB internal SRAM, ~%ums buffer",
             (unsigned)PCM_STREAM_SIZE,
             (unsigned)(PCM_STREAM_SIZE * 1000 / (44100 * 4)));

    BaseType_t ret = xTaskCreatePinnedToCore(
        i2s_feeder_task, "audio_feed",
        board_task_audio_feed.stack_size,
        nullptr,
        board_task_audio_feed.priority,
        &s_feed_task,
        board_task_audio_feed.core_id);
    configASSERT(ret == pdPASS);

    SYS_LOGI(kTag, "I2S device found, feeder task started on Core %d prio %d",
             board_task_audio_feed.core_id, board_task_audio_feed.priority);
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

        xStreamBufferSend(s_pcm_stream, m_buffer, size, portMAX_DELAY);
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