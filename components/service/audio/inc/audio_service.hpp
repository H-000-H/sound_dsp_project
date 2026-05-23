#pragma once

#include <cstdint>

#include "capability/audio_engine.hpp"
#include "device.h"

class AudioService
{
public:
    static AudioService& getInstance();

    bool init();
    bool start();
    void stop();
    void suspend();
    void resume();
    bool is_active() const { return m_inited; }

    void playMp3(uint8_t* data, uint32_t len);
    void setVolume(float volume);

private:
    AudioService() = default;
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    audio_engine_t m_engine = {};
    device_t* m_amp_dev = nullptr;
    bool m_inited = false;
    bool m_started = false;
};
