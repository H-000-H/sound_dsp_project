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
    if (m_state != ModuleState::Created && m_state != ModuleState::Stopped)
    {
        return true;
    }

    lv_init();
    m_state = ModuleState::Initialized;
    EventBus::getInstance().post(SystemEvent::UiReady);
    return true;
}

bool UiService::start()
{
    if (m_state == ModuleState::Created && !init())
    {
        return false;
    }

    m_state = ModuleState::Started;
    return true;
}

void UiService::stop()
{
    m_state = ModuleState::Stopped;
}

void UiService::suspend()
{
    m_state = ModuleState::Suspended;
}

void UiService::resume()
{
    m_state = ModuleState::Started;
}

ModuleState UiService::state() const
{
    return m_state;
}

void UiService::run()
{
    start();
    if (s_ui_entry)
    {
        s_ui_entry();
    }
}
