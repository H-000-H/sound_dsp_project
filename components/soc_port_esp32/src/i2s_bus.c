#include "hal_i2s_bus.h"

#include "driver/i2s_std.h"
#include "osal.h"
#include "VFS.h"
#include <string.h>
#include "board_config.h"

static const char* kTag = "hal_i2s_bus";

typedef struct {
    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;
    int sample_rate;
    int bits_per_sample;
    int pool_idx;
} hal_i2s_impl_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
static hal_i2s_impl_t s_i2s_pool[I2S_COUNT];
static uint8_t s_i2s_used[I2S_COUNT];

static void i2s_release_impl(hal_i2s_impl_t* impl)
{
    if (!impl) return;
    if (impl->tx_handle) {
        i2s_channel_disable(impl->tx_handle);
        i2s_del_channel(impl->tx_handle);
        impl->tx_handle = NULL;
    }
    if (impl->rx_handle) {
        i2s_channel_disable(impl->rx_handle);
        i2s_del_channel(impl->rx_handle);
        impl->rx_handle = NULL;
    }
}

static int i2s_init_impl(hal_i2s_bus_t* bus, const hal_i2s_config_t* cfg)
{
    if (bus == NULL || cfg == NULL) {
        return -1;
    }

    int impl_idx = osal_pool_claim(s_i2s_used, I2S_COUNT);
    if (impl_idx < 0) {
        DRV_LOGE(kTag, "impl pool exhausted");
        return VFS_ERR_NOMEM;
    }
    hal_i2s_impl_t* impl = &s_i2s_pool[impl_idx];
    memset(impl, 0, sizeof(*impl));
    impl->pool_idx = impl_idx;

    int ret = 0;
    impl->sample_rate = cfg->sample_rate;
    impl->bits_per_sample = cfg->bits_per_sample;

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear = true,
    };

    esp_err_t esp_ret = i2s_new_channel(&chan_cfg,
                                     (cfg->dout_pin >= 0) ? &impl->tx_handle : NULL,
                                     (cfg->din_pin >= 0) ? &impl->rx_handle : NULL);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "i2s_new_channel failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_pool;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = cfg->sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = (cfg->bits_per_sample == 24) ? I2S_DATA_BIT_WIDTH_24BIT : I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = (cfg->bits_per_sample == 24) ? I2S_SLOT_BIT_WIDTH_24BIT : I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = (cfg->channel_format == 0) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO,
            .slot_mask = (cfg->dout_pin >= 0) ? I2S_STD_SLOT_BOTH : I2S_STD_SLOT_LEFT,
            .ws_width = (cfg->bits_per_sample == 24) ? 24 : 16,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)cfg->bclk_pin,
            .ws = (gpio_num_t)cfg->ws_pin,
            .dout = (gpio_num_t)cfg->dout_pin,
            .din = (gpio_num_t)cfg->din_pin,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    if (impl->tx_handle) {
        esp_ret = i2s_channel_init_std_mode(impl->tx_handle, &std_cfg);
        if (esp_ret != ESP_OK) {
            DRV_LOGE(kTag, "i2s tx init failed: %d", esp_ret);
            ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
            goto err_chan;
        }
        esp_ret = i2s_channel_enable(impl->tx_handle);
        if (esp_ret != ESP_OK) {
            DRV_LOGE(kTag, "i2s tx enable failed: %d", esp_ret);
            ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
            goto err_chan;
        }
    }

    if (impl->rx_handle) {
        esp_ret = i2s_channel_init_std_mode(impl->rx_handle, &std_cfg);
        if (esp_ret != ESP_OK) {
            DRV_LOGE(kTag, "i2s rx init failed: %d", esp_ret);
            ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
            goto err_chan;
        }
        esp_ret = i2s_channel_enable(impl->rx_handle);
        if (esp_ret != ESP_OK) {
            DRV_LOGE(kTag, "i2s rx enable failed: %d", esp_ret);
            ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
            goto err_chan;
        }
    }

    bus->_impl = impl;
    return 0;

err_chan:
    i2s_release_impl(impl);
err_pool:
    osal_pool_release(s_i2s_used, I2S_COUNT, impl_idx);
    return ret;
}

static int i2s_write_impl(hal_i2s_bus_t* bus, const int16_t* samples,
                           size_t bytes, size_t* written, uint32_t timeout_ms)
{
    if (bus == NULL || bus->_impl == NULL || samples == NULL) {
        return VFS_ERR_INVAL;
    }

    hal_i2s_impl_t* impl = (hal_i2s_impl_t*)bus->_impl;
    size_t bytes_written = 0;

    esp_err_t ret = i2s_channel_write(impl->tx_handle, samples, bytes,
                                       &bytes_written, osal_ticks_from_ms(timeout_ms));
    if (written) {
        *written = bytes_written;
    }

    return (ret == ESP_OK) ? 0 : (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
}

static int i2s_deinit_impl(hal_i2s_bus_t* bus)
{
    if (bus == NULL || bus->_impl == NULL) {
        return -1;
    }

    hal_i2s_impl_t* impl = (hal_i2s_impl_t*)bus->_impl;
    i2s_release_impl(impl);
    osal_pool_release(s_i2s_used, I2S_COUNT, impl->pool_idx);
    bus->_impl = NULL;
    return 0;
}

void hal_i2s_bus_init_struct(hal_i2s_bus_t* bus)
{
    if (bus == NULL) return;
    bus->init = i2s_init_impl;
    bus->write = i2s_write_impl;
    bus->deinit = i2s_deinit_impl;
    bus->_impl = NULL;
}

#include "driver.h"

typedef struct {
    hal_i2s_bus_t bus;
    int pool_idx;
} i2s_bus_priv_t;

static i2s_bus_priv_t s_i2s_bus_priv_pool[I2S_COUNT];
static uint8_t s_i2s_bus_priv_used[I2S_COUNT];

static int i2s_fops_write(device_t* dev, const void* buffer, size_t len, uint32_t timeout_ms)
{
    i2s_bus_priv_t* priv = (i2s_bus_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->bus.write(&priv->bus, (const int16_t*)buffer, len, NULL, timeout_ms);
}

static int i2s_fops_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    i2s_bus_priv_t* priv = (i2s_bus_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case I2S_CMD_WRITE: {
        if (arg_len != sizeof(i2s_write_arg_t) || !arg) return VFS_ERR_INVAL;
        i2s_write_arg_t* a = (i2s_write_arg_t*)arg;
        return priv->bus.write(&priv->bus, a->samples, a->bytes, a->written, a->timeout_ms);
    }
    case I2S_CMD_DEINIT:
        return priv->bus.deinit(&priv->bus);
    default:
        return -1;
    }
}

static const file_operation_t i2s_fops = {
    .write = i2s_fops_write,
    .ioctl = i2s_fops_ioctl,
};

static int i2s_probe(device_t* dev)
{
    int ws, bclk, dout, din = -1, sample_rate = 44100, bits = 16;
    device_get_prop_int(dev, "ws", &ws);
    device_get_prop_int(dev, "bclk", &bclk);
    device_get_prop_int(dev, "dout", &dout);
    device_get_prop_int(dev, "din", &din);
    device_get_prop_int(dev, "sample_rate", &sample_rate);
    device_get_prop_int(dev, "bits_per_sample", &bits);

    if (ws < 0 || bclk < 0 || dout < 0 || sample_rate <= 0 || (bits != 16 && bits != 24)) {
        return -1;
    }

    int pool_idx = osal_pool_claim(s_i2s_bus_priv_used, I2S_COUNT);
    if (pool_idx < 0) return VFS_ERR_NOMEM;
    i2s_bus_priv_t* priv = &s_i2s_bus_priv_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->pool_idx = pool_idx;

    hal_i2s_config_t cfg = {
        .ws_pin = ws, .bclk_pin = bclk, .dout_pin = dout, .din_pin = din,
        .sample_rate = sample_rate, .bits_per_sample = bits,
        .channel_format = 1,
    };

    hal_i2s_bus_init_struct(&priv->bus);
    int ret = priv->bus.init(&priv->bus, &cfg);
    if (ret != 0) {
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &i2s_fops;
    DRV_LOGI(kTag, "I2S probed: ws=%d bclk=%d dout=%d rate=%d", ws, bclk, dout, sample_rate);
    return 0;

err_pool:
    osal_pool_release(s_i2s_bus_priv_used, I2S_COUNT, pool_idx);
    return ret;
}

static int i2s_remove(device_t* dev)
{
    i2s_bus_priv_t* priv = (i2s_bus_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->bus.deinit(&priv->bus);
        osal_pool_release(s_i2s_bus_priv_used, I2S_COUNT, priv->pool_idx);
        device_ops_unregister(dev);
    }
    return 0;
}
