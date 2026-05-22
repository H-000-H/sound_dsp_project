#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_GPIO_MODE_INPUT = 0,
    HAL_GPIO_MODE_OUTPUT,
    HAL_GPIO_MODE_INPUT_OUTPUT,
} hal_gpio_mode_t;

typedef enum {
    HAL_GPIO_PULL_DISABLE = 0,
    HAL_GPIO_PULL_ENABLE,
} hal_gpio_pull_t;

typedef enum {
    HAL_GPIO_INTR_DISABLE = 0,
    HAL_GPIO_INTR_RISING,
    HAL_GPIO_INTR_FALLING,
    HAL_GPIO_INTR_ANY_EDGE,
} hal_gpio_intr_t;

typedef struct {
    int pin;
    hal_gpio_mode_t mode;
    hal_gpio_pull_t pullup;
    hal_gpio_pull_t pulldown;
    hal_gpio_intr_t intr_type;
} hal_gpio_config_t;

typedef void (*hal_gpio_isr_t)(void* arg);

int hal_gpio_init(const hal_gpio_config_t* cfg);
int hal_gpio_set_level(int pin, int level);
int hal_gpio_get_level(int pin);
int hal_gpio_toggle(int pin);
int hal_gpio_install_isr(int isr_flags);
int hal_gpio_add_isr(int pin, hal_gpio_isr_t handler, void* arg);
int hal_gpio_remove_isr(int pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
