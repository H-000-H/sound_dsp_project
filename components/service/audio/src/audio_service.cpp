#include "audio_service.hpp"

#include "event_bus.hpp"
#include "max98357a_driver.h"
#include "mp3.hpp"
#include "system_log.hpp"

static constexpr const char* kTag = "AudioService";

AudioService& AudioService::getInstance()
{
    static AudioService service;
    return service;
}

bool AudioService::init()
{
    if (m_state != ModuleState::Created && m_state != ModuleState::Stopped)
    {
        return true;
    }

    /* 从 DeviceTree 查找功放设备 (driver 已 probe, 硬件已就绪) */
    m_amp_dev = device_find("speaker_amp0");
    if (m_amp_dev == nullptr)
    {
        m_state = ModuleState::Failed;
        SYS_LOGE(kTag, "speaker_amp0 not found in device tree");
        return false;
    }

    if (max98357a_init(m_amp_dev) != 0)
    {
        m_state = ModuleState::Failed;
        SYS_LOGE(kTag, "amp init failed");
        return false;
    }

    MP3::getinstance().init();
    m_state = ModuleState::Initialized;
    EventBus::getInstance().post(SystemEvent::AudioReady);
    return true;
}

bool AudioService::start()
{
    if (m_state == ModuleState::Created && !init())
    {
        return false;
    }

    max98357a_set_enable(m_amp_dev, 1);
    m_state = ModuleState::Started;
    return true;
}

void AudioService::stop()
{
    max98357a_set_enable(m_amp_dev, 0);
    m_state = ModuleState::Stopped;
}

void AudioService::suspend()
{
    max98357a_set_enable(m_amp_dev, 0);
    m_state = ModuleState::Suspended;
}

void AudioService::resume()
{
    max98357a_set_enable(m_amp_dev, 1);
    m_state = ModuleState::Started;
}

ModuleState AudioService::state() const
{
    return m_state;
}

void AudioService::playMp3(uint8_t* data, uint32_t len)
{
    if (data == nullptr || len == 0)
    {
        return;
    }

    start();
    EventBus::getInstance().post(SystemEvent::MusicPlay);
    MP3::getinstance().play(data, len);
}

void AudioService::setVolume(float volume)
{
    MP3::getinstance().MP3_volume(volume);
}
