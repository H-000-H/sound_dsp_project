#pragma once

#include <cstdint>

/**
 * @brief 按键输入适配器 — 取代旧的 device_hal Button 类
 *
 * 内部通过 device_find("buttons0") + gpio_key_scan 实现,
 * 向上提供与旧 Button 兼容的 process() / get_pressed_gpio() 接口,
 * 使 LVGL 层无需大幅修改。
 */
class KeyInput
{
public:
    static KeyInput& getInstance();

    /** 扫描按键（非阻塞）, 应在主循环中周期性调用 */
    void process(uint32_t timeout_ms);

    /** 返回当前按下的按键 GPIO 编号, 无按键时返回 -1 */
    int get_pressed_gpio() const { return m_last_gpio; }

    /** 从 DeviceTree 读取各按键 GPIO (线程安全, 只读一次后缓存) */
    static int getGpioNext();
    static int getGpioPrev();
    static int getGpioEnter();
    static int getGpioEsc();

private:
    KeyInput() = default;
    ~KeyInput() = default;
    KeyInput(const KeyInput&) = delete;
    KeyInput& operator=(const KeyInput&) = delete;

    void* m_dev = nullptr;
    int m_last_gpio = -1;
};
