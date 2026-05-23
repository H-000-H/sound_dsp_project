#include "system_runtime.hpp"

#include "audio_service.hpp"
#include "task_config.h"
#include "cloud_service.hpp"
#include "device.h"
#include "driver.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "event_bus.hpp"
#include "system_log.hpp"
#include "task_manager.hpp"
#include "ui_service.hpp"

static constexpr const char* kTag = "SystemRuntime";

static void ui_task_entry(void* param)
{
    static_cast<UiService*>(param)->run();
}

static void cloud_task_entry(void* param)
{
    static_cast<CloudService*>(param)->run();
}

SystemRuntime& SystemRuntime::getInstance()
{
    static SystemRuntime runtime;
    return runtime;
}

bool SystemRuntime::init()
{
    if (m_state == ModuleState::Initialized) return true;
    if (!can_transit(m_state, ModuleState::Initialized))
    {
        m_state = ModuleState::Failed;
        return false;
    }

    /* ── 系统级网络栈初始化 (为 WiFi / TCP/IP 做准备) ── */
    esp_netif_init();
    esp_event_loop_create_default();

    /* ── DeviceTree + Driver 模型初始化 ── */
    SYS_LOGI(kTag, "=== DeviceTree init ===");
    if (device_tree_init() != 0)
    {
        SYS_LOGE(kTag, "device_tree_init failed");
        /* 不阻断, 旧代码仍可工作 */
    }

    SYS_LOGI(kTag, "=== Driver probe ===");
    board_driver_probe_all();

    EventBus::getInstance().post(SystemEvent::Boot);

    if (!AudioService::getInstance().init())
    {
        m_state = ModuleState::Failed;
        SYS_LOGE(kTag, "audio service init failed");
        return false;
    }

    if (!UiService::getInstance().init())
    {
        m_state = ModuleState::Failed;
        SYS_LOGE(kTag, "ui service init failed");
        return false;
    }

    if (!CloudService::getInstance().init())
    {
        m_state = ModuleState::Failed;
        SYS_LOGE(kTag, "cloud service init failed");
        return false;
    }

    m_state = ModuleState::Initialized;
    EventBus::getInstance().post(SystemEvent::SystemReady);
    return true;
}

bool SystemRuntime::start()
{
    if (m_state == ModuleState::Started) return true;
    if (!can_transit(m_state, ModuleState::Started))
    {
        if ((m_state == ModuleState::Created || m_state == ModuleState::Stopped) && init())
        {
            /* fall through */
        }
        else
        {
            return false;
        }
    }

    AudioService::getInstance().start();

    if (TaskManager::create(board_task_ui, ui_task_entry, &UiService::getInstance()) == nullptr)
    {
        m_state = ModuleState::Failed;
        return false;
    }

    if (TaskManager::create(board_task_cloud, cloud_task_entry, &CloudService::getInstance()) == nullptr)
    {
        m_state = ModuleState::Failed;
        return false;
    }

    m_state = ModuleState::Started;
    return true;
}

void SystemRuntime::stop()
{
    if (!can_transit(m_state, ModuleState::Stopped)) return;
    CloudService::getInstance().stop();
    UiService::getInstance().stop();
    AudioService::getInstance().stop();
    m_state = ModuleState::Stopped;
}

void SystemRuntime::suspend()
{
    if (!can_transit(m_state, ModuleState::Suspended)) return;
    CloudService::getInstance().suspend();
    UiService::getInstance().suspend();
    AudioService::getInstance().suspend();
    m_state = ModuleState::Suspended;
}

void SystemRuntime::resume()
{
    if (!can_transit(m_state, ModuleState::Started)) return;
    AudioService::getInstance().resume();
    UiService::getInstance().resume();
    CloudService::getInstance().resume();
    m_state = ModuleState::Started;
}

ModuleState SystemRuntime::state() const
{
    return m_state;
}
