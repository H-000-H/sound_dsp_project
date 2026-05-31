#include "hal_rmt_led.h"

#include "driver/rmt_tx.h"
#include "osal.h"
#include "VFS.h"
#include <string.h>

static const char* kTag = "hal_rmt_led";

#define WS2812_T0H_TICKS 4
#define WS2812_T0L_TICKS 9
#define WS2812_T1H_TICKS 8
#define WS2812_T1L_TICKS 4

typedef struct {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    uint8_t brightness;
    uint8_t cur_r;
    uint8_t cur_g;
    uint8_t cur_b;
} hal_rmt_led_impl_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define RMT_LED_IMPL_POOL_SIZE 4
static hal_rmt_led_impl_t s_rmt_led_pool[RMT_LED_IMPL_POOL_SIZE];
static uint8_t s_rmt_led_used[RMT_LED_IMPL_POOL_SIZE];

static int rmt_led_init_impl(hal_rmt_led_t* led, int gpio_num, uint32_t resolution_hz)
{
    if (led == NULL) {
        return -1;
    }

    int impl_idx = osal_pool_claim(s_rmt_led_used, RMT_LED_IMPL_POOL_SIZE);
    if (impl_idx < 0) {
        DRV_LOGE(kTag, "impl pool exhausted");
        return VFS_ERR_NOMEM;
    }
    hal_rmt_led_impl_t* impl = &s_rmt_led_pool[impl_idx];
    memset(impl, 0, sizeof(*impl));

    int ret = 0;
    impl->brightness = 128;

    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = (gpio_num_t)gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t esp_ret = rmt_new_tx_channel(&chan_cfg, &impl->channel);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "rmt_new_tx_channel failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_pool;
    }

    rmt_copy_encoder_config_t enc_cfg = {0};
    esp_ret = rmt_new_copy_encoder(&enc_cfg, &impl->encoder);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "rmt_new_copy_encoder failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_channel;
    }

    esp_ret = rmt_enable(impl->channel);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "rmt_enable failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_encoder;
    }

    led->_impl = impl;
    return 0;

err_encoder:
    rmt_del_encoder(impl->encoder);
err_channel:
    rmt_del_channel(impl->channel);
err_pool:
    osal_pool_release(s_rmt_led_used, RMT_LED_IMPL_POOL_SIZE, impl_idx);
    return ret;
}

static int rmt_led_set_rgb_impl(hal_rmt_led_t* led, uint8_t r, uint8_t g, uint8_t b)
{
    if (led == NULL || led->_impl == NULL) {
        return -1;
    }

    hal_rmt_led_impl_t* impl = (hal_rmt_led_impl_t*)led->_impl;
    impl->cur_r = r;
    impl->cur_g = g;
    impl->cur_b = b;

    uint8_t wr = (r * impl->brightness) / 255;
    uint8_t wg = (g * impl->brightness) / 255;
    uint8_t wb = (b * impl->brightness) / 255;

    uint8_t data[3] = {wg, wr, wb};
    rmt_symbol_word_t symbols[24];

    for (int byte_idx = 0; byte_idx < 3; byte_idx++) {
        uint8_t byte = data[byte_idx];
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            int idx = byte_idx * 8 + (7 - bit_idx);
            if (byte & (1 << bit_idx)) {
                symbols[idx].level0 = 1;
                symbols[idx].duration0 = WS2812_T1H_TICKS;
                symbols[idx].level1 = 0;
                symbols[idx].duration1 = WS2812_T1L_TICKS;
            } else {
                symbols[idx].level0 = 1;
                symbols[idx].duration0 = WS2812_T0H_TICKS;
                symbols[idx].level1 = 0;
                symbols[idx].duration1 = WS2812_T0L_TICKS;
            }
        }
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .flags = { .eot_level = 0 },
    };

    esp_err_t ret = rmt_transmit(impl->channel, impl->encoder, symbols, sizeof(symbols), &tx_cfg);
    if (ret != ESP_OK) return (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;

    ret = rmt_tx_wait_all_done(impl->channel, osal_ticks_from_ms(100));
    return (ret == ESP_OK) ? 0 : (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
}

static int rmt_led_set_brightness_impl(hal_rmt_led_t* led, uint8_t brightness)
{
    if (led == NULL || led->_impl == NULL) {
        return -1;
    }

    hal_rmt_led_impl_t* impl = (hal_rmt_led_impl_t*)led->_impl;
    impl->brightness = brightness;

    return rmt_led_set_rgb_impl(led, impl->cur_r, impl->cur_g, impl->cur_b);
}

static int rmt_led_off_impl(hal_rmt_led_t* led)
{
    return rmt_led_set_rgb_impl(led, 0, 0, 0);
}

static int rmt_led_deinit_impl(hal_rmt_led_t* led)
{
    if (led == NULL || led->_impl == NULL) return -1;
    hal_rmt_led_impl_t* impl = (hal_rmt_led_impl_t*)led->_impl;
    rmt_disable(impl->channel);
    if (impl->encoder) rmt_del_encoder(impl->encoder);
    if (impl->channel) rmt_del_channel(impl->channel);
    for (int i = 0; i < RMT_LED_IMPL_POOL_SIZE; i++) { if (&s_rmt_led_pool[i] == impl) { osal_pool_release(s_rmt_led_used, RMT_LED_IMPL_POOL_SIZE, i); break; } }
    led->_impl = NULL;
    return 0;
}

void hal_rmt_led_init_struct(hal_rmt_led_t* led)
{
    if (led == NULL) return;
    led->init = rmt_led_init_impl;
    led->set_rgb = rmt_led_set_rgb_impl;
    led->set_brightness = rmt_led_set_brightness_impl;
    led->off = rmt_led_off_impl;
    led->deinit = rmt_led_deinit_impl;
    led->_impl = NULL;
}

#include "driver.h"

typedef struct {
    hal_rmt_led_t led;
} rmt_led_priv_t;

#define RMT_LED_PRIV_POOL_SIZE 2
static rmt_led_priv_t s_rmt_led_priv_pool[RMT_LED_PRIV_POOL_SIZE];
static uint8_t s_rmt_led_priv_used[RMT_LED_PRIV_POOL_SIZE];

static int rmt_fops_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len, uint32_t timeout_ms)
{
    (void)arg_len; (void)timeout_ms;
    rmt_led_priv_t* priv = (rmt_led_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case RMT_CMD_INIT: {
        if (!arg) return -1;
        rmt_init_arg_t* a = (rmt_init_arg_t*)arg;
        if (priv->led._impl) return 0;
        return priv->led.init(&priv->led, a->gpio, a->resolution_hz);
    }
    case RMT_CMD_SET_RGB: {
        if (!arg) return -1;
        rmt_rgb_arg_t* a = (rmt_rgb_arg_t*)arg;
        return priv->led.set_rgb(&priv->led, a->r, a->g, a->b);
    }
    case RMT_CMD_SET_BRIGHT:
        if (!arg) return -1;
        return priv->led.set_brightness(&priv->led, *(uint8_t*)arg);
    case RMT_CMD_OFF:
        return priv->led.off(&priv->led);
    case RMT_CMD_DEINIT:
        if (!priv->led._impl) return 0;
        return priv->led.deinit(&priv->led);
    default:
        return -1;
    }
}

static int rmt_fops_write(device_t* dev, const void* buf, size_t len, uint32_t timeout_ms);

static const file_operation_t rmt_fops = {
    .write = rmt_fops_write,
    .ioctl = rmt_fops_ioctl,
};

static int rmt_fops_write(device_t* dev, const void* buf, size_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    rmt_led_priv_t* priv = (rmt_led_priv_t*)device_get_priv(dev);
    if (!priv || !buf || len < 3) return VFS_ERR_INVAL;

    /* buf[0..2] = GRB (WS2812 native order) */
    uint8_t g = ((const uint8_t*)buf)[0];
    uint8_t r = ((const uint8_t*)buf)[1];
    uint8_t b = ((const uint8_t*)buf)[2];

    return priv->led.set_rgb(&priv->led, r, g, b);
}

static int rmt_led_probe(device_t* dev)
{
    int gpio = -1;
    uint32_t resolution_hz = 10U * 1000U * 1000U;
    device_get_prop_int(dev, "gpio", &gpio);
    device_get_prop_int(dev, "rmt_resolution_hz", (int*)&resolution_hz);

    if (gpio < 0) {
        DRV_LOGE(kTag, "missing gpio property");
        return VFS_ERR_INVAL;
    }

    int pool_idx = osal_pool_claim(s_rmt_led_priv_used, RMT_LED_PRIV_POOL_SIZE);
    if (pool_idx < 0) return VFS_ERR_NOMEM;
    rmt_led_priv_t* priv = &s_rmt_led_priv_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));

    hal_rmt_led_init_struct(&priv->led);

    int ret = priv->led.init(&priv->led, gpio, resolution_hz);
    if (ret != 0) {
        DRV_LOGE(kTag, "init failed: %d", ret);
        osal_pool_release(s_rmt_led_priv_used, RMT_LED_PRIV_POOL_SIZE, pool_idx);
        return ret;
    }

    /* 亮度 255 = 透传: WS2812 驱动自行处理 brightness 缩放 */
    priv->led.set_brightness(&priv->led, 255);

    device_set_priv(dev, priv);
    dev->ops = &rmt_fops;
    DRV_LOGI(kTag, "RMT LED platform driver probed: GPIO=%d, res=%u", gpio, resolution_hz);
    return 0;
}

static int rmt_led_remove(device_t* dev)
{
    rmt_led_priv_t* priv = (rmt_led_priv_t*)device_get_priv(dev);
    if (priv) {
        if (priv->led._impl) {
            priv->led.off(&priv->led);
            priv->led.deinit(&priv->led);
        }
        for (int i = 0; i < RMT_LED_PRIV_POOL_SIZE; i++) { if (&s_rmt_led_priv_pool[i] == priv) { osal_pool_release(s_rmt_led_priv_used, RMT_LED_PRIV_POOL_SIZE, i); break; } }
        device_ops_unregister(dev);
    }
    return 0;
}

DRIVER_REGISTER(rmt_led, "esp32,rmt-tx", rmt_led_probe, rmt_led_remove);
