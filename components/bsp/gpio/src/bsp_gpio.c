#include "bsp_gpio.h"
#include <string.h>
#if CONFIG_ENABLE_BSP_GPIO
static bool is_gpio_irq_register = false;

void gpio_driver_init(uint32_t gpio_num, gpio_mode_t mode, gpio_pullup_t pull_up_en, gpio_pulldown_t pull_down_en, gpio_int_type_t intr_type)
{
    gpio_config_t config;
    memset(&config, 0, sizeof(config));
    config.pin_bit_mask = (1ULL << gpio_num);
    config.mode = mode;
    config.pull_up_en = pull_up_en;
    config.pull_down_en = pull_down_en;
    config.intr_type = intr_type;
    gpio_config(&config);
}

int gpio_driver_get_level(uint32_t gpio_num)
{
    return gpio_get_level((gpio_num_t)gpio_num);
}

void gpio_driver_set_level(uint32_t gpio_num, uint32_t level)
{
    gpio_set_level((gpio_num_t)gpio_num, level);
}

void gpio_driver_toggle_level(uint32_t gpio_num)
{
    gpio_set_level((gpio_num_t)gpio_num, !gpio_get_level((gpio_num_t)gpio_num));
}

void led_driver_init(uint32_t gpio_num)
{
    gpio_driver_init(gpio_num, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE);
}

void led_driver_on(uint32_t gpio_num)
{
    gpio_set_level((gpio_num_t)gpio_num, 1);
}

void led_driver_off(uint32_t gpio_num)
{
    gpio_set_level((gpio_num_t)gpio_num, 0);
}

void led_driver_toggle(uint32_t gpio_num)
{
    gpio_set_level((gpio_num_t)gpio_num, !gpio_get_level((gpio_num_t)gpio_num));
}

/**
 * @brief 安装中断服务
 * @note  只需要安装一次，多次调用无效
 */
void gpio_driver_install_isr_service()
{
    if (is_gpio_irq_register)
    {
        return;
    }
    gpio_install_isr_service(0);
    is_gpio_irq_register = true;
}

void gpio_driver_add_isr_handler(uint32_t gpio_num, gpio_isr_t handler, void *args)
{
    gpio_isr_handler_add((gpio_num_t)gpio_num, handler, args);
}

void gpio_driver_remove_isr_handler(uint32_t gpio_num)
{
    gpio_isr_handler_remove((gpio_num_t)gpio_num);
}

void gpio_driver_intr_enable(uint32_t gpio_num)
{
    gpio_intr_enable((gpio_num_t)gpio_num);
}

void gpio_driver_intr_disable(uint32_t gpio_num)
{
    gpio_intr_disable((gpio_num_t)gpio_num);
}
#endif