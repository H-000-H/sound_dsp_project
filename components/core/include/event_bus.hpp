#pragma once

#include <cstddef>
#include <cstdint>

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

    bool subscribe(SystemEvent id, EventCallback callback, void* user_data = nullptr);
    void post(SystemEvent id, uintptr_t arg = 0);

private:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    struct Subscriber
    {
        SystemEvent id = SystemEvent::Boot;
        EventCallback callback = nullptr;
        void* user_data = nullptr;
    };

    static constexpr size_t kMaxSubscribers = 24;
    Subscriber m_subscribers[kMaxSubscribers] = {};
    size_t m_count = 0;
};

#ifdef __cplusplus
extern "C" {
#endif
void event_bus_signal_device_removed(void* dev);
#ifdef __cplusplus
}
#endif
