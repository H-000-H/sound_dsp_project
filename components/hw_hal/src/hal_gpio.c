#include "hal_gpio.h"

#include "driver/gpio.h"
#include "esp_err.h"

int hal_gpio_init(const hal_gpio_config_t* cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
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

    return gpio_config(&gpio_cfg);
}

int hal_gpio_set_level(int pin, int level)
{
    return gpio_set_level((gpio_num_t)pin, level);
}

int hal_gpio_get_level(int pin)
{
    return gpio_get_level((gpio_num_t)pin);
}

int hal_gpio_toggle(int pin)
{
    int level = gpio_get_level((gpio_num_t)pin);
    return gpio_set_level((gpio_num_t)pin, !level);
}

int hal_gpio_install_isr(int isr_flags)
{
    esp_err_t ret = gpio_install_isr_service(isr_flags);
    return (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) ? 0 : ret;
}

int hal_gpio_add_isr(int pin, hal_gpio_isr_t handler, void* arg)
{
    return gpio_isr_handler_add((gpio_num_t)pin, handler, arg);
}

int hal_gpio_remove_isr(int pin)
{
    return gpio_isr_handler_remove((gpio_num_t)pin);
}

/* ===== GPIO controller 平台驱动层 ===== */
#include "driver.h"

typedef struct {
    int dummy;
} gpio_ctrl_priv_t;

static int8_t gpio_ctrl_ioctl(device_t* dev, int cmd, void* arg)
{
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
    default:
        return -1;
    }
}

static const file_operation_t gpio_ctrl_fops = {
    .ioctl = gpio_ctrl_ioctl,
};

static int gpio_ctrl_probe(device_t* dev)
{
    gpio_ctrl_priv_t* priv = (gpio_ctrl_priv_t*)calloc(1, sizeof(gpio_ctrl_priv_t));
    if (!priv) return -1;
    device_set_priv(dev, priv);
    dev->ops = &gpio_ctrl_fops;
    return 0;
}

static int gpio_ctrl_remove(device_t* dev)
{
    gpio_ctrl_priv_t* priv = (gpio_ctrl_priv_t*)device_get_priv(dev);
    if (priv) {
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(gpio, "esp32,gpio", gpio_ctrl_probe, gpio_ctrl_remove);
