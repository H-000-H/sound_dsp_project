#include "button.hpp"
#if CONFIG_ENABLE_DEVICE_HAL_BUTTON

/* ISR 中继 — arg 为 GPIO 编号 */
static void IRAM_ATTR button_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    Button::get_instance().on_gpio_interrupt_from_isr(gpio_num);
}

/*====================================================================*/
/*                     内部辅助                                       */
/*====================================================================*/

Button::PinContext* Button::find_pin(uint32_t gpio_num)
{
    for (size_t i = 0; i < m_pin_count; i++)
    {
        if (m_pins[i].gpio_num == gpio_num) return &m_pins[i];
    }
    return nullptr;
}

const Button::PinContext* Button::find_pin(uint32_t gpio_num) const
{
    for (size_t i = 0; i < m_pin_count; i++)
    {
        if (m_pins[i].gpio_num == gpio_num) return &m_pins[i];
    }
    return nullptr;
}

bool Button::is_pressed_level(const PinContext* pin) const
{
    const int level = GpioController::get_instance().get_level(pin->gpio_num);
    return pin->active_low ? (level == 0) : (level != 0);
}

void Button::configure_gpio(uint32_t gpio_num)
{
    GpioController::get_instance().init(
        gpio_num,
        GpioMode::Input,
        GpioPull::Enable,
        GpioPull::Disable,
        GpioInterrupt::AnyEdge);

    GpioController::add_isr_handler(gpio_num, button_isr_handler, (void*)(uintptr_t)gpio_num);
}

/*====================================================================*/
/*                     公有接口                                       */
/*====================================================================*/

Button& Button::get_instance()
{
    static Button instance;
    return instance;
}

/* 单按键初始化（向后兼容）*/
void Button::init(uint32_t gpio_num)
{
    init(&gpio_num, 1);
}

/* 多按键初始化 */
void Button::init(const uint32_t gpio_nums[], size_t count)
{
    if (count > MAX_BUTTONS) count = MAX_BUTTONS;

    /* 首次初始化时创建队列 */
    if (irq_queue == nullptr)
    {
        irq_queue = xQueueCreate(16, sizeof(ButtonIrqEvent));
    }

    /* 首次初始化时安装 ISR 服务（仅一次）*/
    static bool isr_installed = false;
    if (!isr_installed)
    {
        GpioController::install_isr_service();
        isr_installed = true;
    }

    for (size_t i = 0; i < count; i++)
    {
        /* 避免重复注册 */
        if (find_pin(gpio_nums[i]) != nullptr) continue;

        m_pins[m_pin_count].gpio_num = gpio_nums[i];
        m_pins[m_pin_count].state = ButtonState::Idle;
        m_pins[m_pin_count].event = ButtonEvent::None;
        m_pin_count++;

        configure_gpio(gpio_nums[i]);
    }
}

/* ISR 处理 — 仅入队，由 process() 在任务上下文中处理 */
void Button::on_gpio_interrupt_from_isr(uint32_t gpio_num)
{
    if (irq_queue == nullptr) return;

    ButtonIrqEvent irq_event;
    irq_event.gpio_num = gpio_num;
    irq_event.tick_ms  = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;

    BaseType_t higher = pdFALSE;
    xQueueSendFromISR(irq_queue, &irq_event, &higher);
    if (higher == pdTRUE)
    {
        portYIELD_FROM_ISR(higher);
    }
}

/* 非阻塞主处理 — 1) 排空队列  2) 更新消抖状态机  3) 检测长按 */
void Button::process(uint32_t timeout_ms)
{
    /* ── 1. 排空队列中的中断事件 ── */
    ButtonIrqEvent irq_evt;

    /* 先尝试等待一个事件（阻塞 timeout_ms）*/
    if (xQueueReceive(irq_queue, &irq_evt, pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
    {
        PinContext* pin = find_pin(irq_evt.gpio_num);
        if (pin)
        {
            bool pressed = is_pressed_level(pin);
            if (pressed)
            {
                if (pin->state == ButtonState::Idle)
                    pin->state = ButtonState::DebouncePress;
                else if (pin->state == ButtonState::DebounceRelease)
                    pin->state = ButtonState::Press; /* 去伪释放，回到按下 */
                pin->debounce_start = irq_evt.tick_ms;
            }
            else
            {
                if (pin->state == ButtonState::Press || pin->state == ButtonState::LongPress)
                    pin->state = ButtonState::DebounceRelease;
                else if (pin->state == ButtonState::DebouncePress)
                    pin->state = ButtonState::Idle; /* 去伪按下 */
                pin->debounce_start = irq_evt.tick_ms;
            }
        }
    }

    /* 排空剩余队列（非阻塞）*/
    while (xQueueReceive(irq_queue, &irq_evt, 0) == pdTRUE)
    {
        PinContext* pin = find_pin(irq_evt.gpio_num);
        if (!pin) continue;

        bool pressed = is_pressed_level(pin);
        if (pressed)
        {
            if (pin->state == ButtonState::Idle)
                pin->state = ButtonState::DebouncePress;
            else if (pin->state == ButtonState::DebounceRelease)
                pin->state = ButtonState::Press;
            pin->debounce_start = irq_evt.tick_ms;
        }
        else
        {
            if (pin->state == ButtonState::Press || pin->state == ButtonState::LongPress)
                pin->state = ButtonState::DebounceRelease;
            else if (pin->state == ButtonState::DebouncePress)
                pin->state = ButtonState::Idle;
            pin->debounce_start = irq_evt.tick_ms;
        }
    }

    /* ── 2. 处理消抖超时 ── */
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    for (size_t i = 0; i < m_pin_count; i++)
    {
        PinContext* pin = &m_pins[i];

        switch (pin->state)
        {
        case ButtonState::DebouncePress:
            if (now - pin->debounce_start >= pin->debounce_ms)
            {
                if (is_pressed_level(pin))
                {
                    pin->state      = ButtonState::Press;
                    pin->press_start = now;
                    pin->long_sent  = false;
                    pin->event      = ButtonEvent::Press;
                }
                else
                {
                    pin->state = ButtonState::Idle; /* 噪声 */
                }
            }
            break;

        case ButtonState::DebounceRelease:
            if (now - pin->debounce_start >= pin->debounce_ms)
            {
                if (!is_pressed_level(pin))
                {
                    /* 确认释放 */
                    pin->event = pin->long_sent
                                 ? ButtonEvent::Release
                                 : ButtonEvent::short_press;
                    pin->state = ButtonState::Idle;
                }
                else
                {
                    pin->state = ButtonState::Press; /* 噪声，回到按下 */
                }
            }
            break;

        case ButtonState::Press:
            /* 长按检测 */
            if (!pin->long_sent && (now - pin->press_start >= pin->long_press_ms))
            {
                if (is_pressed_level(pin))
                {
                    pin->long_sent = true;
                    pin->state     = ButtonState::LongPress;
                    pin->event     = ButtonEvent::long_press;
                }
            }
            break;

        default:
            break;
        }
    }
}

/* 返回当前确认按下的 GPIO，-1 表示无 */
int Button::get_pressed_gpio()
{
    for (size_t i = 0; i < m_pin_count; i++)
    {
        if (m_pins[i].state == ButtonState::Press ||
            m_pins[i].state == ButtonState::LongPress)
        {
            return m_pins[i].gpio_num;
        }
    }
    return -1;
}

/* 消费式读取事件 */
ButtonEvent Button::get_event(uint32_t gpio_num)
{
    PinContext* pin = find_pin(gpio_num);
    if (!pin) return ButtonEvent::None;

    ButtonEvent evt = pin->event;
    pin->event = ButtonEvent::None;
    return evt;
}

#endif
