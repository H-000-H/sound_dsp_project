#pragma once

#include <cstdint>

#include "capability/led_engine.hpp"

class CloudService
{
public:
    static CloudService& getInstance();

    bool init();
    bool start();
    void stop();
    void suspend();
    void resume();
    bool is_active() const { return m_inited; }

    void run();

private:
    CloudService() = default;
    CloudService(const CloudService&) = delete;
    CloudService& operator=(const CloudService&) = delete;

    bool m_inited = false;
    bool m_started = false;
    led_engine_t m_led;
};
