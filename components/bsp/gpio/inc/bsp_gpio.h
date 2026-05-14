#ifndef __BSP_GPIO_H__
#define __BSP_GPIO_H__
#include "config.hpp"
#if CONFIG_ENABLE_BSP_GPIO
#include <stdint.h>
#include <driver/gpio.h>
#ifdef __cplusplus
extern "C"
{
#endif

void gpio_driver_init(uint32_t gpio_num, gpio_mode_t mode, gpio_pullup_t pull_up_en, gpio_pulldown_t pull_down_en, gpio_int_type_t intr_type);
int gpio_driver_get_level(uint32_t gpio_num);
void gpio_driver_set_level(uint32_t gpio_num, uint32_t level);
void gpio_driver_toggle_level(uint32_t gpio_num);

void led_driver_init(uint32_t gpio_num);
void led_driver_on(uint32_t gpio_num);
void led_driver_off(uint32_t gpio_num);
void led_driver_toggle(uint32_t gpio_num);

void gpio_driver_install_isr_service();
void gpio_driver_add_isr_handler(uint32_t gpio_num, gpio_isr_t handler, void *args);
void gpio_driver_remove_isr_handler(uint32_t gpio_num);
void gpio_driver_intr_enable(uint32_t gpio_num);
void gpio_driver_intr_disable(uint32_t gpio_num);
#ifdef __cplusplus
}
#endif
#endif
#endif
