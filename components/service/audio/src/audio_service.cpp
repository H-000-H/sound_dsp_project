#include "audio_service.hpp"

#include "event_bus.hpp"
#include "system_log.hpp"

static constexpr const char* kTag = "AudioService";

AudioService& AudioService::getInstance()
{
    static AudioService service;
    return service;
}

bool AudioService::init()
{
    if (m_inited) return true;

    audio_engine_init_struct(&m_engine);
    if (m_engine.init(&m_engine) != 0)
    {
        SYS_LOGE(kTag, "audio engine init failed");
        return false;
    }

    m_inited = true;
    EventBus::getInstance().post(SystemEvent::AudioReady);
    return true;
}

bool AudioService::start()
{
    if (m_started) return true;
    if (!m_inited && !init()) return false;

    m_engine.set_enable(&m_engine, 1);
    m_started = true;
    return true;
}

void AudioService::stop()
{
    if (!m_inited) return;
    m_engine.set_enable(&m_engine, 0);
    m_started = false;
}

void AudioService::suspend()
{
    if (!m_started) return;
    m_engine.set_enable(&m_engine, 0);
    m_started = false;
}

void AudioService::resume()
{
    if (m_started) return;
    if (!m_inited) return;
    m_engine.set_enable(&m_engine, 1);
    m_started = true;
}

void AudioService::playMp3(uint8_t* data, uint32_t len)
{
    if (data == nullptr || len == 0) return;
    start();
    EventBus::getInstance().post(SystemEvent::MusicPlay);
    m_engine.play(&m_engine, data, len);
}

void AudioService::setVolume(float volume)
{
    m_engine.set_volume(&m_engine, volume);
}
