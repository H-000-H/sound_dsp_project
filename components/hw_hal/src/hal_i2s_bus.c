#include "hal_i2s_bus.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char* kTag = "hal_i2s_bus";

typedef struct {
    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;
    int sample_rate;
    int bits_per_sample;
} hal_i2s_impl_t;

static int i2s_init_impl(hal_i2s_bus_t* bus, const hal_i2s_config_t* cfg)
{
    if (bus == NULL || cfg == NULL) {
        return -1;
    }

    hal_i2s_impl_t* impl = (hal_i2s_impl_t*)calloc(1, sizeof(hal_i2s_impl_t));
    if (impl == NULL) {
        ESP_LOGE(kTag, "malloc impl failed");
        return -1;
    }

    impl->sample_rate = cfg->sample_rate;
    impl->bits_per_sample = cfg->bits_per_sample;

    /* Channel config */
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear = true,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg,
                                     (cfg->dout_pin >= 0) ? &impl->tx_handle : NULL,
                                     (cfg->din_pin >= 0) ? &impl->rx_handle : NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "i2s_new_channel failed: %d", ret);
        free(impl);
        return ret;
    }

    /* Standard I2S config */
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
        ret = i2s_channel_init_std_mode(impl->tx_handle, &std_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "i2s tx init failed: %d", ret);
            i2s_del_channel(impl->tx_handle);
            free(impl);
            return ret;
        }
        ret = i2s_channel_enable(impl->tx_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "i2s tx enable failed: %d", ret);
            i2s_del_channel(impl->tx_handle);
            free(impl);
            return ret;
        }
    }

    if (impl->rx_handle) {
        ret = i2s_channel_init_std_mode(impl->rx_handle, &std_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "i2s rx init failed: %d", ret);
            if (impl->tx_handle) i2s_del_channel(impl->tx_handle);
            free(impl);
            return ret;
        }
        ret = i2s_channel_enable(impl->rx_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(kTag, "i2s rx enable failed: %d", ret);
            if (impl->tx_handle) i2s_del_channel(impl->tx_handle);
            free(impl);
            return ret;
        }
    }

    bus->_impl = impl;
    return 0;
}

static int i2s_write_impl(hal_i2s_bus_t* bus, const int16_t* samples,
                           size_t bytes, size_t* written, uint32_t timeout_ms)
{
    if (bus == NULL || bus->_impl == NULL || samples == NULL) {
        return -1;
    }

    hal_i2s_impl_t* impl = (hal_i2s_impl_t*)bus->_impl;
    size_t bytes_written = 0;

    esp_err_t ret = i2s_channel_write(impl->tx_handle, samples, bytes,
                                       &bytes_written, pdMS_TO_TICKS(timeout_ms));
    if (written) {
        *written = bytes_written;
    }

    return (ret == ESP_OK) ? 0 : ret;
}

static int i2s_deinit_impl(hal_i2s_bus_t* bus)
{
    if (bus == NULL || bus->_impl == NULL) {
        return -1;
    }

    hal_i2s_impl_t* impl = (hal_i2s_impl_t*)bus->_impl;
    if (impl->tx_handle) {
        i2s_channel_disable(impl->tx_handle);
        i2s_del_channel(impl->tx_handle);
    }
    if (impl->rx_handle) {
        i2s_channel_disable(impl->rx_handle);
        i2s_del_channel(impl->rx_handle);
    }
    free(impl);
    bus->_impl = NULL;
    return 0;
}

/* Initialize a hal_i2s_bus_t struct with default function pointers */
void hal_i2s_bus_init_struct(hal_i2s_bus_t* bus)
{
    if (bus == NULL) return;
    bus->init = i2s_init_impl;
    bus->write = i2s_write_impl;
    bus->deinit = i2s_deinit_impl;
    bus->_impl = NULL;
}
