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

/* ===== UART 平台驱动层 ===== */
#include "driver.h"

typedef struct {
    hal_uart_t uart;
    hal_uart_config_t cfg;
} uart_priv_t;

static int8_t uart_fops_write(device_t* dev, const void* buffer, size_t len)
{
    uart_priv_t* priv = (uart_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->uart.write(&priv->uart, (const uint8_t*)buffer, len);
}

static int8_t uart_fops_ioctl(device_t* dev, int cmd, void* arg)
{
    uart_priv_t* priv = (uart_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case UART_CMD_READ: {
        uart_read_arg_t* a = (uart_read_arg_t*)arg;
        return priv->uart.read(&priv->uart, a->data, a->len, a->timeout_ms);
    }
    case UART_CMD_DEINIT: {
        int ret = priv->uart.deinit(&priv->uart);
        free(priv);
        device_set_priv(dev, NULL);
        return ret;
    }
    case UART_CMD_SET_BAUD: {
        if (!arg) return -1;
        priv->uart.deinit(&priv->uart);
        priv->cfg.baud_rate = *(int*)arg;
        return priv->uart.init(&priv->uart, &priv->cfg);
    }
    default:
        return -1;
    }
}

static const file_operation_t uart_fops = {
    .write = uart_fops_write,
    .ioctl = uart_fops_ioctl,
};

static int uart_probe(device_t* dev)
{
    int tx = 43, rx = 44, baud = 115200, data_bits = 8, stop_bits = 1, parity = 0;
    device_get_prop_int(dev, "tx_pin", &tx);
    device_get_prop_int(dev, "rx_pin", &rx);
    device_get_prop_int(dev, "baud_rate", &baud);
    device_get_prop_int(dev, "data_bits", &data_bits);
    device_get_prop_int(dev, "stop_bits", &stop_bits);
    device_get_prop_int(dev, "parity", &parity);

    uart_priv_t* priv = (uart_priv_t*)calloc(1, sizeof(uart_priv_t));
    if (!priv) return -1;

    priv->cfg.tx_pin = tx; priv->cfg.rx_pin = rx;
    priv->cfg.rts_pin = -1; priv->cfg.cts_pin = -1;
    priv->cfg.baud_rate = baud;
    priv->cfg.data_bits = data_bits;
    priv->cfg.stop_bits = stop_bits;
    priv->cfg.parity = parity;

    hal_uart_init_struct(&priv->uart);
    int ret = priv->uart.init(&priv->uart, &priv->cfg);
    if (ret != 0) { free(priv); return ret; }

    device_set_priv(dev, priv);
    dev->ops = &uart_fops;
    ESP_LOGI(kTag, "UART probed: TX=%d RX=%d baud=%d", tx, rx, baud);
    return 0;
}

static int uart_remove(device_t* dev)
{
    uart_priv_t* priv = (uart_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->uart.deinit(&priv->uart);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(uart, "esp32,uart", uart_probe, uart_remove);
