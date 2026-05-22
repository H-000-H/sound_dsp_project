#include "gpio_key_driver.h"

#include "driver.h"
#include "hal_gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>

static const char* kTag = "gpio_key";

#define MAX_KEYS       4
#define KEY_NAME_LEN   16

typedef struct {
    char           name[KEY_NAME_LEN];
    int            gpio_pin;
    int            pressed_level;     /* 按下时的电平 */
    uint32_t       debounce_ms;
    uint32_t       last_change_tick;  /* 上次变化时间 (ms) */
    int            last_raw;          /* 上次原始电平 */
    int            current_state;     /* 当前确定状态: 1=按下 */
    int            long_press_reported;
} key_entry_t;

typedef struct {
    key_entry_t keys[MAX_KEYS];
    int         key_count;
} gpio_key_priv_t;

/* 获取 tick in ms */
static uint32_t current_tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static int gpio_key_probe(device_t* dev)
{
    /* 读取属性: next_pin, prev_pin, enter_pin, esc_pin, debounce_ms */
    int debounce = 50;
    device_get_prop_int(dev, "debounce_ms", &debounce);

    gpio_key_priv_t* priv = (gpio_key_priv_t*)calloc(1, sizeof(gpio_key_priv_t));
    if (!priv) return -1;

    /* 解析按键引脚: "next_pin", "prev_pin", "enter_pin", "esc_pin" */
    struct { const char* key; const char* name; } pin_names[] = {
        { "next_pin", "next" },
        { "prev_pin", "prev" },
        { "enter_pin", "enter" },
        { "esc_pin", "esc" },
    };
    int default_pressed_level = 0;  /* 默认低电平按下 */

    for (int i = 0; i < MAX_KEYS; i++) {
        int pin = -1;
        device_get_prop_int(dev, pin_names[i].key, &pin);
        if (pin < 0) continue;

        key_entry_t* k = &priv->keys[priv->key_count];

        strncpy(k->name, pin_names[i].name, KEY_NAME_LEN - 1);
        k->gpio_pin = pin;
        k->pressed_level = default_pressed_level;
        k->debounce_ms = debounce;
        k->current_state = 0;

        /* 初始化 GPIO 输入上拉 */
        hal_gpio_config_t cfg = {
            .pin = pin,
            .mode = HAL_GPIO_MODE_INPUT,
            .pullup = HAL_GPIO_PULL_ENABLE,
            .pulldown = HAL_GPIO_PULL_DISABLE,
        };
        hal_gpio_init(&cfg);

        ESP_LOGI(kTag, "  key '%s' = GPIO%d", k->name, pin);
        priv->key_count++;
    }

    if (priv->key_count == 0) {
        ESP_LOGW(kTag, "no keys found, probe anyway");
    }

    device_set_priv(dev, priv);
    ESP_LOGI(kTag, "probed: %d keys, debounce=%dms", priv->key_count, debounce);
    return 0;
}

static int gpio_key_remove(device_t* dev)
{
    gpio_key_priv_t* priv = (gpio_key_priv_t*)device_get_priv(dev);
    if (priv) {
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(gpio_key, "gpio-keys", gpio_key_probe, gpio_key_remove);

/* ── 公开 API ── */
int gpio_key_init(device_t* dev)
{
    return 0;
}

int gpio_key_scan(device_t* dev, gpio_key_state_t* out, int max_keys)
{
    gpio_key_priv_t* priv = (gpio_key_priv_t*)device_get_priv(dev);
    if (!priv || !out || max_keys <= 0) return 0;

    uint32_t now = current_tick_ms();
    int count = 0;

    for (int i = 0; i < priv->key_count && count < max_keys; i++) {
        key_entry_t* k = &priv->keys[i];
        int raw = hal_gpio_get_level(k->gpio_pin);
        int pressed = (raw == k->pressed_level);
        int stable = 0;

        /* 消抖逻辑 */
        if (k->last_raw != pressed) {
            k->last_change_tick = now;
            k->last_raw = pressed;
        }

        uint32_t elapsed = now - k->last_change_tick;
        if (elapsed >= k->debounce_ms) {
            stable = 1;
            if (pressed != k->current_state) {
                k->current_state = pressed;
                if (pressed) {
                    k->long_press_reported = 0;
                }
            }
        }

        if (!stable) continue;

        strncpy(out[count].name, k->name, KEY_NAME_LEN - 1);
        out[count].gpio_pin = k->gpio_pin;

        if (k->current_state) {
            uint32_t press_duration = now - k->last_change_tick;
            out[count].press_ms = press_duration;
            out[count].event = KEY_EVENT_PRESS;

            if (press_duration > 1000 && !k->long_press_reported) {
                out[count].event = KEY_EVENT_LONG_PRESS;
                k->long_press_reported = 1;
            }
        } else {
            out[count].press_ms = 0;
            out[count].event = KEY_EVENT_RELEASE;
        }
        count++;
    }

    return count;
}

int gpio_key_get_count(device_t* dev)
{
    gpio_key_priv_t* priv = (gpio_key_priv_t*)device_get_priv(dev);
    return priv ? priv->key_count : 0;
}
