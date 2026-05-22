#pragma once

#include "lifecycle.hpp"

class CloudService : public Lifecycle
{
public:
    static CloudService& getInstance();

    bool init() override;
    bool start() override;
    void stop() override;
    void suspend() override;
    void resume() override;
    ModuleState state() const override;

    void run();

private:
    CloudService() = default;
    CloudService(const CloudService&) = delete;
    CloudService& operator=(const CloudService&) = delete;

    ModuleState m_state = ModuleState::Created;
};
