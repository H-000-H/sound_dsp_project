#include "gpio_key_driver.h"

#include "driver.h"
#include "VFS.h"
#include "vfs_gpio.h"
#include "osal.h"
#include <string.h>
#include "board_config.h"
#include "esp_attr.h"

static const char* kTag = "gpio_key";

#define MAX_KEYS            4
#define KEY_NAME_LEN        16
#define KEY_RAW_FIFO_DEPTH  16

/* ── SPSC 无锁 FIFO (ISR Producer → Task Consumer) ── */
typedef struct {
    uint32_t timestamp;
    int      pin;
    int      level;
} key_raw_event_t;

typedef struct {
    key_raw_event_t buf[KEY_RAW_FIFO_DEPTH];
    volatile uint8_t head;
    volatile uint8_t tail;
} key_raw_fifo_t;

static inline int raw_fifo_push(key_raw_fifo_t* f, const key_raw_event_t* e)
{
    uint8_t next = (uint8_t)((f->head + 1) & (KEY_RAW_FIFO_DEPTH - 1));
    if (next == f->tail) return -1;
    f->buf[f->head] = *e;
    __sync_synchronize();
    f->head = next;
    return 0;
}

static inline int raw_fifo_pop(key_raw_fifo_t* f, key_raw_event_t* e)
{
    if (f->head == f->tail) return -1;
    *e = f->buf[f->tail];
    __sync_synchronize();
    f->tail = (uint8_t)((f->tail + 1) & (KEY_RAW_FIFO_DEPTH - 1));
    return 0;
}

typedef struct {
    char           name[KEY_NAME_LEN];
    int            gpio_pin;
    int            pressed_level;
    uint32_t       debounce_ms;
    uint32_t       last_change_tick;
    int            last_raw;
    int            current_state;
    int            long_press_reported;
    key_raw_fifo_t* fifo;
} key_entry_t;

typedef struct {
    device_t*       gpio_dev;
    key_entry_t     keys[MAX_KEYS];
    int             key_count;
    key_raw_fifo_t  raw_fifo;
    uint8_t         lock_storage[OSAL_SPINLOCK_STORAGE_SIZE];
    int             pool_idx;
} gpio_key_priv_t;

/* ── BSS 静态池 ── */
static gpio_key_priv_t s_gpio_key_pool[GPIO_KEY_COUNT];
static uint8_t s_gpio_key_used[GPIO_KEY_COUNT];

static uint32_t current_tick_ms(void)
{
    return osal_time_ms();
}

/* ── ISR 声明 ── */
static void gpio_key_isr_handler(void* arg);

/* ── VFS 操作表 ── */
static int gpio_key_init(device_t* dev);
static int gpio_key_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len, uint32_t timeout_ms);
static const file_operation_t gpio_key_fops = {
    .init  = gpio_key_init,
    .ioctl = gpio_key_ioctl,
};

static int gpio_key_probe(device_t* dev)
{
    int debounce = 50;
    device_get_prop_int(dev, "debounce_ms", &debounce);

    int ret = 0;
    int pool_idx = osal_pool_claim(s_gpio_key_used, GPIO_KEY_COUNT);
    if (pool_idx < 0) {
        ret = VFS_ERR_NOMEM;
        goto err_pool;
    }
    gpio_key_priv_t* priv = &s_gpio_key_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->pool_idx = pool_idx;
    osal_spinlock_init((osal_spinlock_t*)priv->lock_storage);
    priv->gpio_dev = device_get_phandle_dev(dev, "gpio");
    if (!priv->gpio_dev) {
        ret = VFS_ERR_DEFER;
        goto err_pool;
    }

    struct { const char* key; const char* name; } pin_names[] = {
        { "next_pin", "next" },
        { "prev_pin", "prev" },
        { "enter_pin", "enter" },
        { "esc_pin", "esc" },
    };
    int default_pressed_level = 0;
    int isr_installed = 0;

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
        k->fifo = &priv->raw_fifo;

        /* 配置 GPIO 输入 + 双边沿中断 */
        hal_gpio_config_t cfg = {
            .pin = pin,
            .mode = HAL_GPIO_MODE_INPUT,
            .pullup = HAL_GPIO_PULL_ENABLE,
            .pulldown = HAL_GPIO_PULL_DISABLE,
            .intr_type = HAL_GPIO_INTR_ANY_EDGE,
        };
        ret = device_ioctl(priv->gpio_dev, GPIO_CMD_CONFIG, &cfg, sizeof(cfg), 100);
        if (ret != 0) {
            goto err_isr;
        }

        /* 读取初始电平 */
        int init_level = vfs_gpio_get_level(pin);
        k->last_raw = (init_level >= 0) ? (init_level == k->pressed_level) : 0;

        /* 安装 ISR (仅首次) */
        if (!isr_installed) {
            ret = device_ioctl(priv->gpio_dev, GPIO_CMD_INSTALL_ISR, NULL, 0, 100);
            if (ret != 0) {
                DRV_LOGW(kTag, "ISR install failed, falling back to poll");
                goto err_isr;
            }
            isr_installed = 1;
        }

        /* 为每个 GPIO 注册 ISR 回调 */
        gpio_isr_arg_t isr_arg = {
            .pin = pin,
            .handler = gpio_key_isr_handler,
            .arg = k,
        };
        ret = device_ioctl(priv->gpio_dev, GPIO_CMD_ADD_ISR, &isr_arg, sizeof(isr_arg), 100);
        if (ret != 0) {
            DRV_LOGW(kTag, "ISR add failed for pin %d", pin);
            goto err_isr;
        }

        DRV_LOGI(kTag, "  key '%s' = GPIO%d [IRQ]", k->name, pin);
        priv->key_count++;
    }

    if (priv->key_count == 0) {
        DRV_LOGW(kTag, "no keys found, probe anyway");
    }

    device_set_priv(dev, priv);
    dev->ops = &gpio_key_fops;
    DRV_LOGI(kTag, "probed: %d keys, debounce=%dms, mode=ISR+FIFO", priv->key_count, debounce);
    return 0;

err_isr:
    /* 回退: 已配置的 GPIO 保持, 依赖 poll 模式 */
    for (int i = 0; i < priv->key_count; i++) {
        device_ioctl(priv->gpio_dev, GPIO_CMD_REMOVE_ISR,
                     &priv->keys[i].gpio_pin, sizeof(int), 100);
    }
    priv->key_count = 0;
err_pool:
    if (pool_idx >= 0) osal_pool_release(s_gpio_key_used, GPIO_KEY_COUNT, pool_idx);
    return ret;
}

static int gpio_key_remove(device_t* dev)
{
    gpio_key_priv_t* priv = (gpio_key_priv_t*)device_get_priv(dev);
    if (priv) {
        for (int i = 0; i < priv->key_count; i++) {
            device_ioctl(priv->gpio_dev, GPIO_CMD_REMOVE_ISR,
                         &priv->keys[i].gpio_pin, sizeof(int), 100);
        }
        osal_pool_release(s_gpio_key_used, GPIO_KEY_COUNT, priv->pool_idx);
        device_ops_unregister(dev);
    }
    return 0;
}

DRIVER_REGISTER(gpio_key, "gpio-keys", gpio_key_probe, gpio_key_remove);

/* ── ISR: 消抖 + 推入 FIFO ── */
static void IRAM_ATTR gpio_key_isr_handler(void* arg)
{
    key_entry_t* k = (key_entry_t*)arg;
    int raw = vfs_gpio_get_level(k->gpio_pin);
    if (raw < 0) return;

    int pressed = (raw == k->pressed_level);
    uint32_t now = current_tick_ms();

    if (pressed != k->last_raw) {
        uint32_t elapsed = now - k->last_change_tick;
        if (elapsed >= k->debounce_ms) {
            k->last_raw = pressed;
            k->last_change_tick = now;

            key_raw_event_t evt = {
                .timestamp = now,
                .pin = k->gpio_pin,
                .level = pressed,
            };
            (void)raw_fifo_push(k->fifo, &evt);
        }
    }
}

/* ── 公开 API ── */
static int gpio_key_init(device_t* dev)
{
    (void)dev;
    return 0;
}

static int gpio_key_scan(device_t* dev, gpio_key_state_t* out, int max_keys)
{
    gpio_key_priv_t* priv = (gpio_key_priv_t*)device_get_priv(dev);
    if (!priv || !out || max_keys <= 0) return 0;

    uint32_t now = current_tick_ms();

    /* ── 消费 FIFO: 更新每键的 raw 状态 ── */
    key_raw_event_t evt;
    while (raw_fifo_pop(&priv->raw_fifo, &evt) == 0) {
        for (int i = 0; i < priv->key_count; i++) {
            key_entry_t* k = &priv->keys[i];
            if (k->gpio_pin != evt.pin) continue;

            osal_spinlock_lock((osal_spinlock_t*)priv->lock_storage);
            k->last_raw = evt.level;
            k->last_change_tick = evt.timestamp;
            k->current_state = evt.level;
            if (evt.level) {
                k->long_press_reported = 0;
            }
            osal_spinlock_unlock((osal_spinlock_t*)priv->lock_storage);
            break;
        }
    }

    /* ── 轮询补齐: 读取 GPIO 检查无事件键的当前状态 ── */
    int count = 0;
    for (int i = 0; i < priv->key_count && count < max_keys; i++) {
        key_entry_t* k = &priv->keys[i];

        int raw = vfs_gpio_get_level(k->gpio_pin);
        if (raw < 0) continue;
        int pressed = (raw == k->pressed_level);

        osal_spinlock_lock((osal_spinlock_t*)priv->lock_storage);

        if (k->last_raw != pressed) {
            k->last_change_tick = now;
            k->last_raw = pressed;
        }
        uint32_t elapsed = now - k->last_change_tick;
        int stable = (elapsed >= k->debounce_ms) ? 1 : 0;

        if (stable) {
            if (pressed != k->current_state) {
                k->current_state = pressed;
                if (pressed) k->long_press_reported = 0;
            }
            if (k->current_state && !k->long_press_reported &&
                (now - k->last_change_tick) > 1000) {
                k->long_press_reported = 1;
            }

            out[count].name = k->name;
            out[count].gpio_pin = k->gpio_pin;
            if (k->current_state) {
                out[count].press_ms = now - k->last_change_tick;
                out[count].event = k->long_press_reported ? KEY_EVENT_LONG_PRESS : KEY_EVENT_PRESS;
            } else {
                out[count].press_ms = 0;
                out[count].event = KEY_EVENT_RELEASE;
            }
        }

        osal_spinlock_unlock((osal_spinlock_t*)priv->lock_storage);

        if (!stable) continue;
        count++;
    }

    return count;
}

static int gpio_key_get_count(device_t* dev)
{
    gpio_key_priv_t* priv = (gpio_key_priv_t*)device_get_priv(dev);
    return priv ? priv->key_count : 0;
}

/* ── ioctl ── */
static int gpio_key_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    int ret = VFS_ERR_INVAL;

    switch (cmd) {
    case GPIO_KEY_CMD_SCAN:
        if (arg_len != sizeof(gpio_key_scan_arg_t) || !arg) return VFS_ERR_INVAL;
        {
            gpio_key_scan_arg_t* a = (gpio_key_scan_arg_t*)arg;
            ret = gpio_key_scan(dev, a->out, a->max_keys);
        }
        break;
    case GPIO_KEY_CMD_GET_COUNT:
        ret = gpio_key_get_count(dev);
        break;
    default:
        ret = VFS_ERR_INVAL;
        break;
    }
    return ret;
}