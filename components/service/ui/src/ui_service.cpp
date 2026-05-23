#include "ui_service.hpp"

#include "event_bus.hpp"
#include "lvgl.h"

/* 函数指针方式调用 lvgl_main（避免 service → app 直接链接依赖）*/
static void (*s_ui_entry)(void) = nullptr;

void UiService::set_entry(void (*entry)(void))
{
    s_ui_entry = entry;
}

UiService& UiService::getInstance()
{
    static UiService service;
    return service;
}

bool UiService::init()
{
    if (m_inited) return true;

    lv_init();
    m_inited = true;
    EventBus::getInstance().post(SystemEvent::UiReady);
    return true;
}

bool UiService::start()
{
    if (m_started) return true;
    if (!m_inited && !init()) return false;

    m_started = true;
    return true;
}

void UiService::stop()
{
    m_started = false;
}

void UiService::suspend()
{
    m_started = false;
}

void UiService::resume()
{
    if (m_started) return;
    if (!m_inited) return;
    m_started = true;
}

void UiService::run()
{
    start();
    if (s_ui_entry)
    {
        s_ui_entry();
    }
}
