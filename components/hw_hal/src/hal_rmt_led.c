#include "hal_rmt_led.h"

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
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

static int rmt_led_init_impl(hal_rmt_led_t* led, int gpio_num, uint32_t resolution_hz)
{
    if (led == NULL) {
        return -1;
    }

    hal_rmt_led_impl_t* impl = (hal_rmt_led_impl_t*)calloc(1, sizeof(hal_rmt_led_impl_t));
    if (impl == NULL) {
        ESP_LOGE(kTag, "malloc failed");
        return -1;
    }

    impl->brightness = 128;
    impl->cur_r = 0;
    impl->cur_g = 0;
    impl->cur_b = 0;

    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = (gpio_num_t)gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t ret = rmt_new_tx_channel(&chan_cfg, &impl->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "rmt_new_tx_channel failed: %d", ret);
        free(impl);
        return ret;
    }

    rmt_copy_encoder_config_t enc_cfg = {0};
    ret = rmt_new_copy_encoder(&enc_cfg, &impl->encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "rmt_new_copy_encoder failed: %d", ret);
        rmt_del_channel(impl->channel);
        free(impl);
        return ret;
    }

    ret = rmt_enable(impl->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "rmt_enable failed: %d", ret);
        rmt_del_channel(impl->channel);
        free(impl);
        return ret;
    }

    led->_impl = impl;
    return 0;
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

    /* Apply brightness */
    uint8_t wr = (r * impl->brightness) / 255;
    uint8_t wg = (g * impl->brightness) / 255;
    uint8_t wb = (b * impl->brightness) / 255;

    /* WS2812 format: GRB */
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
    if (ret != ESP_OK) return ret;

    return rmt_tx_wait_all_done(impl->channel, pdMS_TO_TICKS(100));
}

static int rmt_led_set_brightness_impl(hal_rmt_led_t* led, uint8_t brightness)
{
    if (led == NULL || led->_impl == NULL) {
        return -1;
    }

    hal_rmt_led_impl_t* impl = (hal_rmt_led_impl_t*)led->_impl;
    impl->brightness = brightness;

    /* Re-apply with current color */
    return rmt_led_set_rgb_impl(led, impl->cur_r, impl->cur_g, impl->cur_b);
}

static int rmt_led_off_impl(hal_rmt_led_t* led)
{
    return rmt_led_set_rgb_impl(led, 0, 0, 0);
}

void hal_rmt_led_init_struct(hal_rmt_led_t* led)
{
    if (led == NULL) return;
    led->init = rmt_led_init_impl;
    led->set_rgb = rmt_led_set_rgb_impl;
    led->set_brightness = rmt_led_set_brightness_impl;
    led->off = rmt_led_off_impl;
    led->_impl = NULL;
}

/* ===== RMT 平台驱动层 ===== */
#include "driver.h"

typedef struct {
    hal_rmt_led_t led;
} rmt_led_priv_t;

static int8_t rmt_fops_ioctl(device_t* dev, int cmd, void* arg)
{
    rmt_led_priv_t* priv = (rmt_led_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case RMT_CMD_SET_RGB: {
        rmt_rgb_arg_t* a = (rmt_rgb_arg_t*)arg;
        return priv->led.set_rgb(&priv->led, a->r, a->g, a->b);
    }
    case RMT_CMD_SET_BRIGHT:
        if (!arg) return -1;
        return priv->led.set_brightness(&priv->led, *(uint8_t*)arg);
    case RMT_CMD_OFF:
        return priv->led.off(&priv->led);
    default:
        return -1;
    }
}

static const file_operation_t rmt_fops = {
    .ioctl = rmt_fops_ioctl,
};

static int rmt_led_probe(device_t* dev)
{
    rmt_led_priv_t* priv = (rmt_led_priv_t*)calloc(1, sizeof(rmt_led_priv_t));
    if (!priv) return -1;

    hal_rmt_led_init_struct(&priv->led);
    device_set_priv(dev, priv);
    dev->ops = &rmt_fops;
    ESP_LOGI(kTag, "RMT LED platform driver probed");
    return 0;
}

static int rmt_led_remove(device_t* dev)
{
    rmt_led_priv_t* priv = (rmt_led_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->led.off(&priv->led);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(rmt_led, "esp32,rmt-tx", rmt_led_probe, rmt_led_remove);
