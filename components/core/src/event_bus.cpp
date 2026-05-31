#include "event_bus.hpp"
#include "safe_state.h"
#include "system_log.hpp"
#include "system_wdt.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static constexpr const char* kTag = "EventBus";
static constexpr uint32_t kDispatchPrio = 6;
static constexpr int kDispatchCore = 0;
static constexpr uint32_t kDispatchStack = 2048;
static constexpr uint32_t kStopWaitMs = 500;

EventBus::EventBus() = default;

bool EventBus::init()
{
    if (m_inited) return true;

    m_queue = xQueueCreate(kQueueLen, sizeof(Event));
    if (m_queue == nullptr)
    {
        SYS_LOGE(kTag, "FATAL: xQueueCreate failed — event bus unusable");
        return false;
    }

    osal_mutex_create_static(&m_sub_lock, m_sub_lock_storage, sizeof(m_sub_lock_storage));
    if (m_sub_lock == nullptr)
    {
        SYS_LOGE(kTag, "FATAL: mutex create failed");
        vQueueDelete(m_queue);
        m_queue = nullptr;
        return false;
    }

    m_inited = true;
    SYS_LOGI(kTag, "event bus initialized, queue=%u slots", (unsigned)kQueueLen);
    return true;
}

EventBus& EventBus::getInstance()
{
    static EventBus bus;
    return bus;
}

bool EventBus::subscribe(SystemEvent id, EventCallback callback, void* user_data)
{
    if (callback == nullptr || m_sub_lock == nullptr)
    {
        return false;
    }

    if (osal_mutex_lock(m_sub_lock, OSAL_LOCK_TIMEOUT_DEFAULT_MS) != 0)
    {
        SYS_LOGE(kTag, "Fatal: EventBus subscribe lock timeout (possible deadlock)");
        return false;
    }

    bool ok = false;
    if (m_count < kMaxSubscribers)
    {
        m_subscribers[m_count].id = id;
        m_subscribers[m_count].callback = callback;
        m_subscribers[m_count].user_data = user_data;
        m_count++;
        ok = true;
    }

    osal_mutex_unlock(m_sub_lock);
    return ok;
}

bool EventBus::post(SystemEvent id, uintptr_t arg)
{
    if (m_queue == nullptr)
    {
        return false;
    }

    /*
     * ⛔ ISR 优先级铁律:
     *   中断优先级 > configMAX_SYSCALL_INTERRUPT_PRIORITY (Level 3)
     *   的 ISR 调用 xQueueSendFromISR 会撕裂 FreeRTOS 内核链表.
     *   此类高优 ISR 只能写裸静态数组, 然后触发低优软件中断转发.
     *   本实现由 osal_in_isr() 自动分流, 不额外做优先级检查.
     */
    const Event event = {id, arg};

    BaseType_t ret;
    if (osal_in_isr())
    {
        BaseType_t high_task_woken = pdFALSE;
        ret = xQueueSendFromISR(m_queue, &event, &high_task_woken);
        if (high_task_woken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
    else
    {
        ret = xQueueSend(m_queue, &event, 0);
    }

    if (ret != pdTRUE)
    {
        __atomic_add_fetch(&m_dropped, 1, __ATOMIC_RELAXED);
        if (!osal_in_isr())
        {
            size_t cur = __atomic_load_n(&m_dropped, __ATOMIC_RELAXED);
            if ((cur % 8) == 0 && cur != 0)
            {
                SYS_LOGW(kTag, "event queue full, dropped=%u", (unsigned)cur);
            }
        }
        return false;
    }
    return true;
}

size_t EventBus::dropped_count() const
{
    return __atomic_load_n(&m_dropped, __ATOMIC_RELAXED);
}

void EventBus::dispatch_task(void* param)
{
    EventBus* self = static_cast<EventBus*>(param);
    Event event;

    while (xQueueReceive(self->m_queue, &event, portMAX_DELAY) == pdTRUE)
    {
        if (event.id == SystemEvent::KillBus)
        {
            break;
        }

        system_wdt_feed();
        system_wdt_feed_rtc();

        Subscriber snapshot[kMaxSubscribers];
        size_t snapshot_count = 0;

        if (self->m_sub_lock)
        {
            if (osal_mutex_lock(self->m_sub_lock, OSAL_LOCK_TIMEOUT_DEFAULT_MS) != 0)
            {
                SYS_LOGE(kTag, "Fatal: EventBus dispatch lock timeout — safe shutdown");
                enter_safe_state("EventBus mutex deadlock");
                break;
            }
        }
        snapshot_count = self->m_count;
        for (size_t i = 0; i < snapshot_count; i++)
        {
            snapshot[i] = self->m_subscribers[i];
        }
        if (self->m_sub_lock)
        {
            osal_mutex_unlock(self->m_sub_lock);
        }

        for (size_t i = 0; i < snapshot_count; i++)
        {
            Subscriber& sub = snapshot[i];
            if (sub.callback != nullptr && sub.id == event.id)
            {
                sub.callback(event, sub.user_data);
            }
        }
    }

    SYS_LOGI(kTag, "dispatch task exiting");
    vTaskDelete(NULL);
}

void EventBus::start()
{
    if (m_task != nullptr || m_queue == nullptr) return;

    xTaskCreatePinnedToCore(dispatch_task, "evt_bus",
                            kDispatchStack, this,
                            kDispatchPrio,
                            (TaskHandle_t*)&m_task,
                            kDispatchCore);
    system_wdt_subscribe((TaskHandle_t)m_task);
    SYS_LOGI(kTag, "dispatch task started on core %d prio %u", kDispatchCore, kDispatchPrio);
}

void EventBus::stop()
{
    if (!m_task) return;

    post(SystemEvent::KillBus);

    TaskHandle_t handle = (TaskHandle_t)m_task;
    m_task = nullptr;

    uint32_t waited = 0;
    while (eTaskGetState(handle) != eDeleted && waited < kStopWaitMs)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        waited += 10;
    }

    if (eTaskGetState(handle) != eDeleted)
    {
        SYS_LOGW(kTag, "dispatch task did not exit, force deleting");
        vTaskDelete(handle);
    }
}

extern "C" void event_bus_signal_device_removed(void* dev)
{
    EventBus::getInstance().post(SystemEvent::DeviceRemoved, (uintptr_t)dev);
}