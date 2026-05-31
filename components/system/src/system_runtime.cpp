#include "system_runtime.hpp"

#include "audio_service.hpp"
#include "task_config.h"
#include "cloud_service.hpp"
#include "critical_data.h"
#include "device.h"
#include "driver.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "event_bus.hpp"
#include "safe_state.h"
#include "system_log.hpp"
#include "system_scrubber.hpp"
#include "system_wdt.hpp"
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

    /* ── Bootloop 防护: 连续崩溃 ≥ 5 次 → 物理锁死, 禁止写 Flash ── */
    if (!safe_state_check_bootloop()) return false;

    /* ── RTC 硬件看门狗: 使用独立 32kHz 时钟, CPU 总线卡死不耽误复位 ── */
    system_wdt_init_rtc(8000);

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
    int probe_fail = board_driver_probe_all();
    if (probe_fail != 0)
    {
        SYS_LOGE(kTag, "board_driver_probe_all: %d device(s) failed", probe_fail);
    }

    /* ── EventBus 两段式初始化: 构造函数为空, init() 在此显式调用 ── */
    if (!EventBus::getInstance().init())
    {
        SYS_LOGE(kTag, "EventBus init failed — entering safe state");
        m_state = ModuleState::Failed;
        enter_safe_state("EventBus init failed");
        return false;
    }

    EventBus::getInstance().post(SystemEvent::Boot);

    if (!AudioService::getInstance().init())
    {
        SYS_LOGE(kTag, "audio service init failed — entering safe state");
        m_state = ModuleState::Failed;
        enter_safe_state("AudioService init failed");
        return false;
    }

    if (!UiService::getInstance().init())
    {
        SYS_LOGE(kTag, "ui service init failed — entering safe state");
        m_state = ModuleState::Failed;
        enter_safe_state("UiService init failed");
        return false;
    }

    if (!CloudService::getInstance().init())
    {
        SYS_LOGE(kTag, "cloud service init failed — entering safe state");
        m_state = ModuleState::Failed;
        enter_safe_state("CloudService init failed");
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

    EventBus::getInstance().start();

    /* ── 全局 Task WDT: 3 秒超时, 超时触发 Core Dump + 硬件复位 ── */
    system_wdt_init(3000);

    AudioService::getInstance().start();

    /* ── 双核隔离: APP_CPU (Core 1) = UI + 音频, PRO_CPU (Core 0) = 网络 ── */
    TaskHandle_t ui_handle = TaskManager::create(board_task_ui, ui_task_entry, &UiService::getInstance());
    if (ui_handle == nullptr)
    {
        SYS_LOGE(kTag, "ui task creation failed — entering safe state");
        m_state = ModuleState::Failed;
        enter_safe_state("UI task create failed");
        return false;
    }
    system_wdt_subscribe(ui_handle);
    system_wdt_stack_monitor_register(ui_handle, 512);

    TaskHandle_t cloud_handle = TaskManager::create(board_task_cloud, cloud_task_entry, &CloudService::getInstance());
    if (cloud_handle == nullptr)
    {
        SYS_LOGE(kTag, "cloud task creation failed — entering safe state");
        m_state = ModuleState::Failed;
        enter_safe_state("Cloud task create failed");
        return false;
    }
    system_wdt_stack_monitor_register(cloud_handle, 512);

    /* ── Flash 位腐烂巡检: 最低优先级 1, 每秒读 1KB 后台 CRC ── */
    system_scrubber_init();
    system_scrubber_start();

    safe_state_clear_bootloop();

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
