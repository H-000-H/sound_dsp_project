#pragma once

#include "lifecycle.hpp"

class UiService : public Lifecycle
{
public:
    static UiService& getInstance();

    static void set_entry(void (*entry)(void));

    bool init() override;
    bool start() override;
    void stop() override;
    void suspend() override;
    void resume() override;
    ModuleState state() const override;

    void run();

private:
    UiService() = default;
    UiService(const UiService&) = delete;
    UiService& operator=(const UiService&) = delete;

    ModuleState m_state = ModuleState::Created;
};
