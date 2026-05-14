#pragma once
#include "config.hpp"
#include "gpio_controller.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstdint>
#include <cstddef>
#if CONFIG_ENABLE_DEVICE_HAL_BUTTON
enum class ButtonState
{
    Idle = 0,         /*空闲状态*/
    DebouncePress,    /*等待按下消抖确认*/
    Press,            /*按键已按下*/
    LongPress,        /*长按状态*/
    DebounceRelease,  /*等待释放消抖确认*/
};

enum class ButtonEvent
{
    None = 0,
    Press,
    short_press,
    long_press,
    Release,
};

struct ButtonIrqEvent
{
    uint32_t gpio_num;
    uint32_t tick_ms;
};

class Button
{
private:
    struct PinContext
    {
        uint32_t gpio_num = 0;
        ButtonState state = ButtonState::Idle;
        ButtonEvent event = ButtonEvent::None;
        bool active_low = true;
        bool long_sent = false;
        uint32_t debounce_start = 0;   /*开始消抖的时间戳(ms)*/
        uint32_t press_start = 0;      /*确认按下时的时间戳(ms)*/
        uint32_t debounce_ms = 20;
        uint32_t long_press_ms = 800;
    };

    static constexpr size_t MAX_BUTTONS = 8;
    PinContext m_pins[MAX_BUTTONS];
    size_t m_pin_count = 0;
    QueueHandle_t irq_queue = nullptr;

    PinContext* find_pin(uint32_t gpio_num);
    const PinContext* find_pin(uint32_t gpio_num) const;
    bool is_pressed_level(const PinContext* pin) const;
    void configure_gpio(uint32_t gpio_num);

    Button() = default;
    ~Button() = default;
    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

public:
    static Button& get_instance();

    /** @brief 初始化单个按键(向后兼容) */
    void init(uint32_t gpio_num);

    /** @brief 初始化多个按键
     *  @param gpio_nums GPIO编号数组
     *  @param count     数组长度
     */
    void init(const uint32_t gpio_nums[], size_t count);

    /** @brief 主处理函数(非阻塞) — 处理队列中的中断事件并更新消抖状态
     *  @param timeout_ms 等待队列事件的最大时间(0=不等待)
     *  @note 需要在任务中周期性调用
     */
    void process(uint32_t timeout_ms = 10);

    /** @brief 获取当前处于按下状态(已消抖)的GPIO编号
     *  @return GPIO编号, -1 表示无按键按下
     *  @note 供LVGL轮询调用
     */
    int get_pressed_gpio();

    /** @brief ISR中断处理(在ISR上下文中调用) */
    void on_gpio_interrupt_from_isr(uint32_t gpio_num);

    /** @brief 获取指定GPIO产生的事件(消费式) */
    ButtonEvent get_event(uint32_t gpio_num);
};
#endif
