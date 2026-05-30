#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    HAL_GPIO_MODE_INPUT = 0,
    HAL_GPIO_MODE_OUTPUT,
    HAL_GPIO_MODE_INPUT_OUTPUT,
} hal_gpio_mode_t;

typedef enum
{
    HAL_GPIO_PULL_DISABLE = 0,
    HAL_GPIO_PULL_ENABLE,
} hal_gpio_pull_t;

typedef enum
{
    HAL_GPIO_INTR_DISABLE = 0,
    HAL_GPIO_INTR_RISING,
    HAL_GPIO_INTR_FALLING,
    HAL_GPIO_INTR_ANY_EDGE,
} hal_gpio_intr_t;

typedef struct
{
    int pin;
    hal_gpio_mode_t mode;
    hal_gpio_pull_t pullup;
    hal_gpio_pull_t pulldown;
    hal_gpio_intr_t intr_type;
} hal_gpio_config_t;

typedef void (*hal_gpio_isr_t)(void* arg);

/* ── 快路径 API（直接函数调用，绕过 device_ioctl） ──
 *   默认启用。定义 HAL_GPIO_FAST_PATH=0 禁止外部直接调 GPIO 函数，
 *   强制走 device_ioctl() 通用路径。
 */
#ifndef HAL_GPIO_FAST_PATH
#define HAL_GPIO_FAST_PATH 1
#endif

#if HAL_GPIO_FAST_PATH
int hal_gpio_init(const hal_gpio_config_t* cfg);
int hal_gpio_set_level(int pin, int level);
int hal_gpio_get_level(int pin);
int hal_gpio_toggle(int pin);
int hal_gpio_install_isr(int isr_flags);
int hal_gpio_add_isr(int pin, hal_gpio_isr_t handler, void* arg);
int hal_gpio_remove_isr(int pin);
#endif

#define GPIO_CMD_CONFIG       0x10
#define GPIO_CMD_TOGGLE       0x11
#define GPIO_CMD_INSTALL_ISR  0x12
#define GPIO_CMD_ADD_ISR      0x13
#define GPIO_CMD_REMOVE_ISR   0x14
#define GPIO_CMD_SET_LEVEL    0x15
#define GPIO_CMD_GET_LEVEL    0x16

typedef struct {
    int pin;
    hal_gpio_isr_t handler;
    void* arg;
} gpio_isr_arg_t;

typedef struct {
    int pin;
    int level;
} gpio_level_arg_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
