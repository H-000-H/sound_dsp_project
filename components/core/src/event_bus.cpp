#include "event_bus.hpp"

EventBus& EventBus::getInstance()
{
    static EventBus bus;
    return bus;
}

bool EventBus::subscribe(SystemEvent id, EventCallback callback, void* user_data)
{
    if (callback == nullptr || m_count >= kMaxSubscribers)
    {
        return false;
    }

    m_subscribers[m_count].id = id;
    m_subscribers[m_count].callback = callback;
    m_subscribers[m_count].user_data = user_data;
    m_count++;
    return true;
}

void EventBus::post(SystemEvent id, uintptr_t arg)
{
    const Event event = {id, arg};
    for (size_t i = 0; i < m_count; i++)
    {
        Subscriber& subscriber = m_subscribers[i];
        if (subscriber.callback != nullptr && subscriber.id == id)
        {
            subscriber.callback(event, subscriber.user_data);
        }
    }
}

extern "C" void event_bus_signal_device_removed(void* dev)
{
    EventBus::getInstance().post(SystemEvent::DeviceRemoved, (uintptr_t)dev);
}
