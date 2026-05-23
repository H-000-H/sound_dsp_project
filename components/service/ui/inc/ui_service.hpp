#pragma once

#include <cstdint>

class UiService
{
public:
    static UiService& getInstance();

    static void set_entry(void (*entry)(void));

    bool init();
    bool start();
    void stop();
    void suspend();
    void resume();
    bool is_active() const { return m_inited; }

    void run();

private:
    UiService() = default;
    UiService(const UiService&) = delete;
    UiService& operator=(const UiService&) = delete;

    bool m_inited = false;
    bool m_started = false;
};
