#include "hal_i2c.h"

#include "device.h"
#include "driver.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "osal.h"
#include "VFS.h"
#include "board_config.h"
#include "esp_rom_sys.h"

#include <stdint.h>
#include <string.h>

static const char* TAG = "hal_i2c";

#define I2C_DEV_CACHE_SIZE 4

typedef struct
{
    uint8_t addr;
    i2c_master_dev_handle_t handle;
} i2c_dev_slot_t;

typedef struct
{
    i2c_master_bus_handle_t bus_handle;
    i2c_dev_slot_t devices[I2C_DEV_CACHE_SIZE];
    uint32_t clock_hz;
    int sda_pin;
    int scl_pin;
    osal_mutex_t* lock;
    int pool_idx;
} hal_i2c_impl_t;

static inline int safe_timeout_ms_to_int(uint32_t timeout_ms)
{
    return (timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)timeout_ms;
}

/* ── BSS 静态池（禁止运行时动态分配, 尺寸由 DTS 编译期确定） ── */
static hal_i2c_impl_t s_i2c_pool[I2C_COUNT];
static uint8_t s_i2c_used[I2C_COUNT];

static int i2c_bus_recover_impl(hal_i2c_bus_t* bus);

static int i2c_ret_to_vfs(esp_err_t ret)
{
    if (ret == ESP_OK) return VFS_OK;
    if (ret == ESP_ERR_TIMEOUT) return VFS_ERR_BUSY;
    return (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
}

static int i2c_get_device(hal_i2c_impl_t* impl, uint8_t addr, i2c_master_dev_handle_t* out)
{
    if (!impl || !out || addr == 0 || addr > 0x7F) return -1;

    for (int i = 0; i < I2C_DEV_CACHE_SIZE; i++) {
        if (impl->devices[i].handle && impl->devices[i].addr == addr) {
            *out = impl->devices[i].handle;
            return 0;
        }
    }

    for (int i = 0; i < I2C_DEV_CACHE_SIZE; i++) {
        if (!impl->devices[i].handle) {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = addr,
                .scl_speed_hz = impl->clock_hz,
            };
            esp_err_t ret = i2c_master_bus_add_device(impl->bus_handle, &dev_cfg,
                                                       &impl->devices[i].handle);
            if (ret != ESP_OK) return i2c_ret_to_vfs(ret);
            impl->devices[i].addr = addr;
            *out = impl->devices[i].handle;
            return 0;
        }
    }

    return -1;
}

static int i2c_init_impl(hal_i2c_bus_t* bus, const hal_i2c_config_t* cfg)
{
    if (!bus || !cfg || cfg->sda_pin < 0 || cfg->scl_pin < 0 || cfg->clock_hz == 0) {
        return -1;
    }

    int impl_idx = osal_pool_claim(s_i2c_used, I2C_COUNT);
    if (impl_idx < 0) return VFS_ERR_NOMEM;
    hal_i2c_impl_t* impl = &s_i2c_pool[impl_idx];
    memset(impl, 0, sizeof(*impl));
    impl->pool_idx = impl_idx;
    int ret = 0;
    impl->clock_hz = cfg->clock_hz;
    impl->sda_pin  = cfg->sda_pin;
    impl->scl_pin  = cfg->scl_pin;

    if (osal_mutex_create(&impl->lock) != 0) {
        ret = VFS_ERR_IO;
        goto err_pool;
    }

    i2c_master_bus_config_t esp_cfg = {
        .i2c_port = cfg->port,
        .sda_io_num = (gpio_num_t)cfg->sda_pin,
        .scl_io_num = (gpio_num_t)cfg->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t esp_ret = i2c_new_master_bus(&esp_cfg, &impl->bus_handle);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(TAG, "i2c_new_master_bus failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_mutex;
    }

    bus->_impl = impl;
    return 0;

err_mutex:
    osal_mutex_destroy(impl->lock);
err_pool:
    osal_pool_release(s_i2c_used, I2C_COUNT, impl_idx);
    return ret;
}

/* ── I2C 读/写/读写 — HAL 层隐式自愈 (Self-Healing)
 * IEC 61508 §7.4.3.4: 驱动封装不得向业务层泄露总线恢复职责.
 * 超时/BUSY 时自动触发 bus_recover → 重试一次 → 失败则向上传播错误.
 */
static int i2c_write_impl(hal_i2c_bus_t* bus, uint8_t addr,
                          const uint8_t* data, size_t len, uint32_t timeout_ms)
{
    if (!bus || !bus->_impl || !data || len == 0) return VFS_ERR_INVAL;
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;

    if (osal_mutex_lock(impl->lock, timeout_ms) != 0) return VFS_ERR_BUSY;
    i2c_master_dev_handle_t dev = NULL;
    int ret = i2c_get_device(impl, addr, &dev);
    if (ret == 0) {
        ret = i2c_ret_to_vfs(i2c_master_transmit(dev, data, len,
                              safe_timeout_ms_to_int(timeout_ms)));
    }
    if (ret == VFS_ERR_BUSY) {
        DRV_LOGW(TAG, "Bus stalled on write, attempting auto-recovery...");
        if (i2c_bus_recover_impl(bus) == 0) {
            ret = i2c_get_device(impl, addr, &dev);
            if (ret == 0) {
                ret = i2c_ret_to_vfs(i2c_master_transmit(dev, data, len,
                                      safe_timeout_ms_to_int(timeout_ms)));
            }
        }
    }
    osal_mutex_unlock(impl->lock);
    return ret;
}

static int i2c_read_impl(hal_i2c_bus_t* bus, uint8_t addr,
                         uint8_t* data, size_t len, uint32_t timeout_ms)
{
    if (!bus || !bus->_impl || !data || len == 0) return VFS_ERR_INVAL;
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;

    if (osal_mutex_lock(impl->lock, timeout_ms) != 0) return VFS_ERR_BUSY;
    i2c_master_dev_handle_t dev = NULL;
    int ret = i2c_get_device(impl, addr, &dev);
    if (ret == 0) {
        ret = i2c_ret_to_vfs(i2c_master_receive(dev, data, len,
                             safe_timeout_ms_to_int(timeout_ms)));
    }
    if (ret == VFS_ERR_BUSY) {
        DRV_LOGW(TAG, "Bus stalled on read, attempting auto-recovery...");
        if (i2c_bus_recover_impl(bus) == 0) {
            ret = i2c_get_device(impl, addr, &dev);
            if (ret == 0) {
                ret = i2c_ret_to_vfs(i2c_master_receive(dev, data, len,
                                      safe_timeout_ms_to_int(timeout_ms)));
            }
        }
    }
    osal_mutex_unlock(impl->lock);
    return ret;
}

static int i2c_read_write_imp(hal_i2c_bus_t* bus, uint8_t addr,
                              const uint8_t* wdata, size_t wlen,
                              uint8_t* rdata, size_t rlen, uint32_t timeout_ms)
{
    if (!bus || !bus->_impl || !wdata || !rdata || wlen == 0 || rlen == 0) return VFS_ERR_INVAL;
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;

    if (osal_mutex_lock(impl->lock, timeout_ms) != 0) return VFS_ERR_BUSY;
    i2c_master_dev_handle_t dev = NULL;
    int ret = i2c_get_device(impl, addr, &dev);
    if (ret == 0) {
        ret = i2c_ret_to_vfs(i2c_master_transmit_receive(dev, wdata, wlen, rdata, rlen,
                              safe_timeout_ms_to_int(timeout_ms)));
    }
    if (ret == VFS_ERR_BUSY) {
        DRV_LOGW(TAG, "Bus stalled on write_read, attempting auto-recovery...");
        if (i2c_bus_recover_impl(bus) == 0) {
            ret = i2c_get_device(impl, addr, &dev);
            if (ret == 0) {
                ret = i2c_ret_to_vfs(i2c_master_transmit_receive(dev, wdata, wlen, rdata, rlen,
                                      safe_timeout_ms_to_int(timeout_ms)));
            }
        }
    }
    osal_mutex_unlock(impl->lock);
    return ret;
}

static int i2c_deinit(hal_i2c_bus_t* bus)
{
    if (!bus || !bus->_impl) return -1;
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;

    for (int i = 0; i < I2C_DEV_CACHE_SIZE; i++) {
        if (impl->devices[i].handle) {
            i2c_master_bus_rm_device(impl->devices[i].handle);
            impl->devices[i].handle = NULL;
        }
    }
    if (impl->bus_handle) {
        i2c_del_master_bus(impl->bus_handle);
    }
    osal_mutex_destroy(impl->lock);
    osal_pool_release(s_i2c_used, I2C_COUNT, impl->pool_idx);
    bus->_impl = NULL;
    return 0;
}

/* ── I2C 总线死锁恢复 (IEC 61508 §7.4.3.4 / IEC 60601-1-2 EMI 抗扰)
 * SDA 被从设备拉低死锁时:
 *   1. 接管 SCL 为 GPIO 输出
 *   2. 发送最多 9 个时钟脉冲, 每脉冲后检测 SDA 是否释放
 *   3. SDA 释放后发送 STOP 条件
 *   4. 重新初始化 I2C 外设
 */
static int i2c_bus_recover_impl(hal_i2c_bus_t* bus)
{
    if (!bus || !bus->_impl) return -1;
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;

    if (impl->sda_pin < 0 || impl->scl_pin < 0) return -1;

    gpio_set_direction(impl->scl_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(impl->sda_pin, GPIO_MODE_INPUT);

    for (int i = 0; i < 9; i++) {
        gpio_set_level(impl->scl_pin, 0);
        esp_rom_delay_us(10);
        gpio_set_level(impl->scl_pin, 1);
        esp_rom_delay_us(10);
        if (gpio_get_level(impl->sda_pin)) break;
    }

    if (gpio_get_level(impl->sda_pin) == 0) {
        DRV_LOGE(TAG, "I2C bus recovery FAILED: SDA still LOW after 9 pulses "
                 "(SDA=%d SCL=%d) — possible short-to-ground",
                 impl->sda_pin, impl->scl_pin);
        gpio_set_direction(impl->scl_pin, GPIO_MODE_INPUT);
        gpio_set_direction(impl->sda_pin, GPIO_MODE_INPUT);
        return VFS_ERR_HW_FATAL;
    }

    gpio_set_direction(impl->sda_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(impl->sda_pin, 0);
    esp_rom_delay_us(10);
    gpio_set_level(impl->scl_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(impl->sda_pin, 1);

    gpio_set_direction(impl->scl_pin, GPIO_MODE_INPUT);
    gpio_set_direction(impl->sda_pin, GPIO_MODE_INPUT);

    if (impl->bus_handle) {
        i2c_del_master_bus(impl->bus_handle);
        impl->bus_handle = NULL;
    }

    i2c_master_bus_config_t esp_cfg = {
        .i2c_port = -1,
        .sda_io_num = (gpio_num_t)impl->sda_pin,
        .scl_io_num = (gpio_num_t)impl->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&esp_cfg, &impl->bus_handle);
    return (ret == ESP_OK) ? 0 : -1;
}

void hal_i2c_init_struct(hal_i2c_bus_t* bus)
{
    if (!bus) return;
    bus->init = i2c_init_impl;
    bus->write = i2c_write_impl;
    bus->read = i2c_read_impl;
    bus->write_read = i2c_read_write_imp;
    bus->bus_recover = i2c_bus_recover_impl;
    bus->deinit = i2c_deinit;
    bus->_impl = NULL;
}

typedef struct {
    hal_i2c_bus_t bus;
    int pool_idx;
} i2c_priv_t;

static i2c_priv_t s_i2c_priv_pool[I2C_COUNT];
static uint8_t s_i2c_priv_used[I2C_COUNT];

static int i2c_fops_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    i2c_priv_t* priv = (i2c_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case I2C_CMD_WRITE: {
        if (arg_len != sizeof(i2c_rw_arg_t) || !arg) return VFS_ERR_INVAL;
        i2c_rw_arg_t* a = (i2c_rw_arg_t*)arg;
        return priv->bus.write(&priv->bus, a->addr, a->data, a->len, a->timeout);
    }
    case I2C_CMD_READ: {
        if (arg_len != sizeof(i2c_rw_arg_t) || !arg) return VFS_ERR_INVAL;
        i2c_rw_arg_t* a = (i2c_rw_arg_t*)arg;
        return priv->bus.read(&priv->bus, a->addr, a->data, a->len, a->timeout);
    }
    case I2C_CMD_WRITE_READ: {
        if (arg_len != sizeof(i2c_wr_arg_t) || !arg) return VFS_ERR_INVAL;
        i2c_wr_arg_t* a = (i2c_wr_arg_t*)arg;
        return priv->bus.write_read(&priv->bus, a->addr, a->wdata, a->wlen,
                                    a->rdata, a->rlen, a->timeout);
    }
    case I2C_CMD_DEINIT:
        return priv->bus.deinit(&priv->bus);
    default:
        return -1;
    }
}

static const file_operation_t i2c_fops = {
    .ioctl = i2c_fops_ioctl,
};

static int i2c_probe(device_t* dev)
{
    int sda = -1, scl = -1, clock = 400000, port = 0;
    device_get_prop_int(dev, "sda_pin", &sda);
    device_get_prop_int(dev, "scl_pin", &scl);
    device_get_prop_int(dev, "clock_hz", &clock);
    device_get_prop_int(dev, "port", &port);
    device_get_prop_int(dev, "reg", &port);

    if (sda < 0 || scl < 0 || clock <= 0) {
        DRV_LOGE(TAG, "missing I2C pin config");
        return -1;
    }

    int pool_idx = osal_pool_claim(s_i2c_priv_used, I2C_COUNT);
    if (pool_idx < 0) return VFS_ERR_NOMEM;
    i2c_priv_t* priv = &s_i2c_priv_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->pool_idx = pool_idx;

    hal_i2c_config_t cfg = {
        .sda_pin = sda,
        .scl_pin = scl,
        .clock_hz = (uint32_t)clock,
        .port = port,
    };

    hal_i2c_init_struct(&priv->bus);
    int ret = priv->bus.init(&priv->bus, &cfg);
    if (ret != 0) {
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &i2c_fops;
    DRV_LOGI(TAG, "I2C probed: port=%d SDA=%d SCL=%d clock=%d", port, sda, scl, clock);
    return 0;

err_pool:
    osal_pool_release(s_i2c_priv_used, I2C_COUNT, pool_idx);
    return ret;
}

static int i2c_remove(device_t* dev)
{
    i2c_priv_t* priv = (i2c_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->bus.deinit(&priv->bus);
        osal_pool_release(s_i2c_priv_used, I2C_COUNT, priv->pool_idx);
        device_ops_unregister(dev);
    }
    return 0;
}

/* ── 强类型 I2C 总线访问器 (替代 ioctl, MISRA C 11.3 合规) ── */
hal_i2c_bus_t* device_get_i2c_bus(device_t* dev)
{
    if (!dev) return NULL;
    i2c_priv_t* priv = (i2c_priv_t*)device_get_priv(dev);
    if (!priv) return NULL;
    return &priv->bus;
}
