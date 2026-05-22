#pragma once

#include <cstdint>

#include "device.h"
#include "lifecycle.hpp"

class AudioService : public Lifecycle
{
public:
    static AudioService& getInstance();

    bool init() override;
    bool start() override;
    void stop() override;
    void suspend() override;
    void resume() override;
    ModuleState state() const override;

    void playMp3(uint8_t* data, uint32_t len);
    void setVolume(float volume);

private:
    AudioService() = default;
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    device_t* m_amp_dev = nullptr;
    ModuleState m_state = ModuleState::Created;
};
