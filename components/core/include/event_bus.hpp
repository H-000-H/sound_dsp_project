#pragma once

#include <cstddef>
#include <cstdint>

#include "osal.h"

enum class SystemEvent : uint32_t
{
    Boot = 0,
    SystemReady,
    UiReady,
    AudioReady,
    CloudReady,
    MusicPlay,
    MusicPause,
    MusicStop,
    DeviceRemoved,
    Fault,
    KillBus,
};

struct Event
{
    SystemEvent id;
    uintptr_t arg;
};

using EventCallback = void (*)(const Event& event, void* user_data);

class EventBus
{
public:
    static EventBus& getInstance();

    /*
     * 两段式初始化 (IEC 61508 §7.4.2.2 SIOF 防御):
     *   构造函数 = default, 不分配任何资源.
     *   init() 必须在 app_main / SystemRuntime::init() 中
     *   按拓扑顺序在最早期显式调用.
     */
    bool init();

    bool subscribe(SystemEvent id, EventCallback callback, void* user_data = nullptr);

    /*
     * 异步投递: 推入 FreeRTOS 队列后立即返回, 由 dispatch_task 在目标上下文执行回调.
     * 返回 false 表示队列满 (高水位告警), 调用方应处理降级策略.
     * ISR 安全: 自动检测中断上下文, 使用 xQueueSendFromISR 路径.
     */
    bool post(SystemEvent id, uintptr_t arg = 0);

    size_t dropped_count() const;

    void start();
    void stop();

private:
    EventBus();
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    struct Subscriber
    {
        SystemEvent id = SystemEvent::Boot;
        EventCallback callback = nullptr;
        void* user_data = nullptr;
    };

    static constexpr size_t kMaxSubscribers = 24;
    static constexpr size_t kQueueLen = 64;

    Subscriber m_subscribers[kMaxSubscribers] = {};
    size_t m_count = 0;
    bool m_inited = false;

    void* m_queue = nullptr;
    void* m_task = nullptr;
    size_t m_dropped = 0;

    osal_mutex_t* m_sub_lock = nullptr;
    uint8_t m_sub_lock_storage[OSAL_MUTEX_STORAGE_SIZE];

    static void dispatch_task(void* param);
};

#ifdef __cplusplus
extern "C" {
#endif
void event_bus_signal_device_removed(void* dev);
#ifdef __cplusplus
}
#endif