#include "hal_gpio.h"

#include "driver/gpio.h"
#include "osal.h"
#include "VFS.h"
#include <string.h>

static int gpio_pin_valid(int pin)
{
    return pin >= 0 && pin < GPIO_NUM_MAX;
}

static int gpio_ret_to_vfs(esp_err_t ret)
{
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) return VFS_OK;
    return (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
}

int hal_gpio_init(const hal_gpio_config_t* cfg)
{
    if (cfg == NULL || !gpio_pin_valid(cfg->pin)) {
        return -1;
    }

    gpio_config_t gpio_cfg =
    {
        .pin_bit_mask = (1ULL << cfg->pin),
        .mode = (cfg->mode == HAL_GPIO_MODE_OUTPUT) ? GPIO_MODE_OUTPUT
               : (cfg->mode == HAL_GPIO_MODE_INPUT_OUTPUT) ? GPIO_MODE_INPUT_OUTPUT
               : GPIO_MODE_INPUT,
        .pull_up_en = (cfg->pullup == HAL_GPIO_PULL_ENABLE) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (cfg->pulldown == HAL_GPIO_PULL_ENABLE) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = (cfg->intr_type == HAL_GPIO_INTR_RISING) ? GPIO_INTR_POSEDGE
                     : (cfg->intr_type == HAL_GPIO_INTR_FALLING) ? GPIO_INTR_NEGEDGE
                     : (cfg->intr_type == HAL_GPIO_INTR_ANY_EDGE) ? GPIO_INTR_ANYEDGE
                     : GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&gpio_cfg);
    return gpio_ret_to_vfs(ret);
}

int hal_gpio_set_level(int pin, int level)
{
    if (!gpio_pin_valid(pin)) return -1;
    return gpio_ret_to_vfs(gpio_set_level((gpio_num_t)pin, level));
}

int hal_gpio_get_level(int pin)
{
    if (!gpio_pin_valid(pin)) return -1;
    return gpio_get_level((gpio_num_t)pin);
}

int hal_gpio_toggle(int pin)
{
    if (!gpio_pin_valid(pin)) return -1;
    int level = gpio_get_level((gpio_num_t)pin);
    return gpio_ret_to_vfs(gpio_set_level((gpio_num_t)pin, !level));
}

int hal_gpio_install_isr(int isr_flags)
{
    return gpio_ret_to_vfs(gpio_install_isr_service(isr_flags));
}

int hal_gpio_add_isr(int pin, hal_gpio_isr_t handler, void* arg)
{
    if (!gpio_pin_valid(pin) || !handler) return -1;
    return gpio_ret_to_vfs(gpio_isr_handler_add((gpio_num_t)pin, handler, arg));
}

int hal_gpio_remove_isr(int pin)
{
    if (!gpio_pin_valid(pin)) return -1;
    return gpio_ret_to_vfs(gpio_isr_handler_remove((gpio_num_t)pin));
}

#include "driver.h"

typedef struct {
    int dummy;
} gpio_ctrl_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define GPIO_CTRL_PRIV_POOL_SIZE 2
static gpio_ctrl_priv_t s_gpio_ctrl_pool[GPIO_CTRL_PRIV_POOL_SIZE];
static uint8_t s_gpio_ctrl_used[GPIO_CTRL_PRIV_POOL_SIZE];

static int gpio_ctrl_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)
{
    (void)arg_len;
    switch (cmd) {
    case GPIO_CMD_CONFIG:
        if (!arg) return -1;
        return hal_gpio_init((const hal_gpio_config_t*)arg);
    case GPIO_CMD_TOGGLE:
        if (!arg) return -1;
        return hal_gpio_toggle(*(int*)arg);
    case GPIO_CMD_INSTALL_ISR:
        if (!arg) return -1;
        return hal_gpio_install_isr(*(int*)arg);
    case GPIO_CMD_ADD_ISR:
        if (!arg) return -1;
        {
            gpio_isr_arg_t* a = (gpio_isr_arg_t*)arg;
            return hal_gpio_add_isr(a->pin, a->handler, a->arg);
        }
    case GPIO_CMD_REMOVE_ISR:
        if (!arg) return -1;
        return hal_gpio_remove_isr(*(int*)arg);
    case GPIO_CMD_SET_LEVEL:
        if (!arg) return -1;
        {
            gpio_level_arg_t* a = (gpio_level_arg_t*)arg;
            return hal_gpio_set_level(a->pin, a->level);
        }
    case GPIO_CMD_GET_LEVEL:
        if (!arg) return -1;
        {
            gpio_level_arg_t* a = (gpio_level_arg_t*)arg;
            a->level = hal_gpio_get_level(a->pin);
            return a->level < 0 ? -1 : 0;
        }
    default:
        return -1;
    }
}

static const file_operation_t gpio_ctrl_fops = {
    .ioctl = gpio_ctrl_ioctl,
};

static int gpio_ctrl_probe(device_t* dev)
{
    int pool_idx = osal_pool_claim(s_gpio_ctrl_used, GPIO_CTRL_PRIV_POOL_SIZE);
    if (pool_idx < 0) return VFS_ERR_NOMEM;

    gpio_ctrl_priv_t* priv = &s_gpio_ctrl_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    if (!priv) return VFS_ERR_NOMEM;
    device_set_priv(dev, priv);
    dev->ops = &gpio_ctrl_fops;
    return 0;
}

static int gpio_ctrl_remove(device_t* dev)
{
    gpio_ctrl_priv_t* priv = (gpio_ctrl_priv_t*)device_get_priv(dev);
    if (priv) {
        for (int i = 0; i < GPIO_CTRL_PRIV_POOL_SIZE; i++) { if (&s_gpio_ctrl_pool[i] == priv) { osal_pool_release(s_gpio_ctrl_used, GPIO_CTRL_PRIV_POOL_SIZE, i); break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(gpio, "esp32,gpio", gpio_ctrl_probe, gpio_ctrl_remove);
