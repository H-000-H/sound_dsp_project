#include "bsp_rgb_led.h"
#if CONFIG_ENABLE_BSP_RGB_LED
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "soc/soc_caps.h"

#define WS2812_T0H_TICKS 4
#define WS2812_T0L_TICKS 9
#define WS2812_T1H_TICKS 8
#define WS2812_T1L_TICKS 4
void rgb_led_init(rgb_led_handle_t* handle, uint32_t gpio_num)
{
    handle->rgb_bright = CONFIG_BSP_RGB_LED_DEFAULT_BRIGHTNESS;
    handle->cur_r = 0;
    handle->cur_g = 0;
    handle->cur_b = 0;
    handle->channel_handle = NULL;
    handle->encoder = NULL;

    rmt_tx_channel_config_t channel_config;
    memset(&channel_config, 0, sizeof(channel_config));
    channel_config.gpio_num = (gpio_num_t)gpio_num;
    #if defined(SOC_RMT_SUPPORT_APB) && SOC_RMT_SUPPORT_APB
    channel_config.clk_src = RMT_CLK_SRC_APB;
    #else
    channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
    #endif
    channel_config.resolution_hz = CONFIG_BSP_RGB_LED_RMT_RESOLUTION_HZ;
    channel_config.mem_block_symbols = 64;
    channel_config.trans_queue_depth = 4;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_config, &handle->channel_handle));

    rmt_copy_encoder_config_t encoder_config;
    memset(&encoder_config, 0, sizeof(encoder_config));
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &handle->encoder));

    ESP_ERROR_CHECK(rmt_enable(handle->channel_handle));
    vTaskDelay(pdMS_TO_TICKS(10));
    rgb_led_off(handle);
}

void rgb_led_set_color(rgb_led_handle_t* handle, uint8_t red, uint8_t green, uint8_t blue)
{
    if (handle->channel_handle == NULL) return;

    handle->cur_r = red;
    handle->cur_g = green;
    handle->cur_b = blue;

    red = (red * handle->rgb_bright) / 255;
    green = (green * handle->rgb_bright) / 255;
    blue = (blue * handle->rgb_bright) / 255;

    uint8_t data[3] = {green, red, blue};
    rmt_symbol_word_t symbols[24];
    int sym_idx = 0;

    for (int byte_idx = 0; byte_idx < 3; byte_idx++)
    {
        uint8_t byte = data[byte_idx];
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--)
        {
            if (byte & (1 << bit_idx))
            {
                symbols[sym_idx].level0 = 1;
                symbols[sym_idx].duration0 = WS2812_T1H_TICKS;
                symbols[sym_idx].level1 = 0;
                symbols[sym_idx].duration1 = WS2812_T1L_TICKS;
            }
            else
            {
                symbols[sym_idx].level0 = 1;
                symbols[sym_idx].duration0 = WS2812_T0H_TICKS;
                symbols[sym_idx].level1 = 0;
                symbols[sym_idx].duration1 = WS2812_T0L_TICKS;
            }
            sym_idx++;
        }
    }

    rmt_transmit_config_t transmit_config;
    memset(&transmit_config, 0, sizeof(transmit_config));
    ESP_ERROR_CHECK(rmt_transmit(handle->channel_handle, handle->encoder, symbols, sizeof(symbols), &transmit_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(handle->channel_handle, pdMS_TO_TICKS(100)));
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void rgb_change(uint32_t color, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b)
{
    *out_r = (color >> 16) & 0xff;
    *out_g = (color >> 8) & 0xff;
    *out_b = color & 0xff;
}

void rgb_led_set_color_u32(rgb_led_handle_t* handle, uint32_t color)
{
    uint8_t r, g, b;
    rgb_change(color, &r, &g, &b);
    rgb_led_set_color(handle, r, g, b);
}

void rgb_led_set_bright(rgb_led_handle_t* handle, uint8_t bright)
{
    handle->rgb_bright = bright;
    rgb_led_set_color(handle, handle->cur_r, handle->cur_g, handle->cur_b);
}

void rgb_led_off(rgb_led_handle_t* handle)
{
    rgb_led_set_color(handle, 0, 0, 0);
}
#endif
