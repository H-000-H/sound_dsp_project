#include "hal_uart.h"

#include "driver/uart.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* kTag = "hal_uart";

typedef struct {
    uart_port_t port;
} hal_uart_impl_t;

static int uart_init_impl(hal_uart_t* uart, const hal_uart_config_t* cfg)
{
    if (uart == NULL || cfg == NULL) {
        return -1;
    }

    hal_uart_impl_t* impl = (hal_uart_impl_t*)calloc(1, sizeof(hal_uart_impl_t));
    if (impl == NULL) {
        ESP_LOGE(kTag, "malloc failed");
        return -1;
    }

    /* Use UART_NUM_1 for auxiliary serial (UART_NUM_0 is usually console) */
    impl->port = UART_NUM_1;

    uart_config_t uart_cfg = {
        .baud_rate = cfg->baud_rate,
        .data_bits = (cfg->data_bits == 5) ? UART_DATA_5_BITS
                    : (cfg->data_bits == 6) ? UART_DATA_6_BITS
                    : (cfg->data_bits == 7) ? UART_DATA_7_BITS
                    : UART_DATA_8_BITS,
        .parity = (cfg->parity == 1) ? UART_PARITY_ODD
                 : (cfg->parity == 2) ? UART_PARITY_EVEN
                 : UART_PARITY_DISABLE,
        .stop_bits = (cfg->stop_bits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(impl->port, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "uart_param_config failed: %d", ret);
        free(impl);
        return ret;
    }

    ret = uart_set_pin(impl->port, cfg->tx_pin, cfg->rx_pin,
                        cfg->rts_pin, cfg->cts_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "uart_set_pin failed: %d", ret);
        free(impl);
        return ret;
    }

    ret = uart_driver_install(impl->port, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "uart_driver_install failed: %d", ret);
        free(impl);
        return ret;
    }

    uart->_impl = impl;
    return 0;
}

static int uart_write_impl(hal_uart_t* uart, const uint8_t* data, size_t len)
{
    if (uart == NULL || uart->_impl == NULL || data == NULL || len == 0) {
        return -1;
    }

    hal_uart_impl_t* impl = (hal_uart_impl_t*)uart->_impl;
    int ret = uart_write_bytes(impl->port, data, len);
    return (ret >= 0) ? 0 : ret;
}

static int uart_read_impl(hal_uart_t* uart, uint8_t* data, size_t len, uint32_t timeout_ms)
{
    if (uart == NULL || uart->_impl == NULL || data == NULL || len == 0) {
        return -1;
    }

    hal_uart_impl_t* impl = (hal_uart_impl_t*)uart->_impl;
    int ret = uart_read_bytes(impl->port, data, len, pdMS_TO_TICKS(timeout_ms));
    return (ret >= 0) ? ret : ret;
}

static int uart_deinit_impl(hal_uart_t* uart)
{
    if (uart == NULL || uart->_impl == NULL) {
        return -1;
    }

    hal_uart_impl_t* impl = (hal_uart_impl_t*)uart->_impl;
    uart_driver_delete(impl->port);
    free(impl);
    uart->_impl = NULL;
    return 0;
}

void hal_uart_init_struct(hal_uart_t* uart)
{
    if (uart == NULL) return;
    uart->init = uart_init_impl;
    uart->write = uart_write_impl;
    uart->read = uart_read_impl;
    uart->deinit = uart_deinit_impl;
    uart->_impl = NULL;
}
