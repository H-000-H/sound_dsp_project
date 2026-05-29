#include "hal_spi_bus.h"

#include "driver/spi_master.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char* kTag = "hal_spi_bus";

typedef struct {
    spi_host_device_t host;
    spi_device_handle_t dev;
} hal_spi_impl_t;

static int spi_init_impl(hal_spi_bus_t* bus, const hal_spi_bus_config_t* bus_cfg,
                         const hal_spi_device_config_t* dev_cfg)
{
    if (bus == NULL || bus_cfg == NULL || dev_cfg == NULL)
    {
        return -1;
    }

    hal_spi_impl_t* impl = (hal_spi_impl_t*)calloc(1, sizeof(hal_spi_impl_t));
    if (impl == NULL) {
        ESP_LOGE(kTag, "malloc impl failed");
        return -1;
    }

    /* Determine SPI host from CS pin or use default SPI2 */
    impl->host = SPI2_HOST;

    spi_bus_config_t esp_bus_cfg = {
        .mosi_io_num = bus_cfg->mosi,
        .miso_io_num = bus_cfg->miso,
        .sclk_io_num = bus_cfg->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (bus_cfg->max_transfer_sz > 0) ? bus_cfg->max_transfer_sz : 4096,
    };

    spi_device_interface_config_t esp_dev_cfg = {
        .mode = dev_cfg->mode,
        .clock_speed_hz = dev_cfg->clock_speed_hz,
        .spics_io_num = dev_cfg->cs_pin,
        .queue_size = dev_cfg->queue_size > 0 ? dev_cfg->queue_size : 7,
    };

    esp_err_t ret = spi_bus_initialize(impl->host, &esp_bus_cfg,
                                        (bus_cfg->dma_chan >= 0) ? bus_cfg->dma_chan : SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "spi_bus_initialize failed: %d", ret);
        free(impl);
        return ret;
    }

    ret = spi_bus_add_device(impl->host, &esp_dev_cfg, &impl->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "spi_bus_add_device failed: %d", ret);
        spi_bus_free(impl->host);
        free(impl);
        return ret;
    }

    bus->_impl = impl;
    return 0;
}

static int spi_write_impl(hal_spi_bus_t* bus, const uint8_t* data, size_t len)
{
    if (bus == NULL || bus->_impl == NULL || data == NULL || len == 0) {
        return -1;
    }

    hal_spi_impl_t* impl = (hal_spi_impl_t*)bus->_impl;

    spi_transaction_t trans = 
    {
        .length = len * 8,
        .tx_buffer = data,
    };

    esp_err_t ret = spi_device_transmit(impl->dev, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "spi_device_transmit failed: %d", ret);
        return ret;
    }
    return 0;
}

static int spi_read_impl(hal_spi_bus_t* bus, uint8_t* data, size_t len)
{
    if (bus == NULL || bus->_impl == NULL || data == NULL || len == 0) {
        return -1;
    }

    hal_spi_impl_t* impl = (hal_spi_impl_t*)bus->_impl;

    spi_transaction_t trans = {
        .length = len * 8,
        .rxlength = len * 8,
        .rx_buffer = data,
    };

    esp_err_t ret = spi_device_transmit(impl->dev, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "spi_device_transmit failed: %d", ret);
        return ret;
    }
    return 0;
}

static int spi_deinit_impl(hal_spi_bus_t* bus)
{
    if (bus == NULL || bus->_impl == NULL) {
        return -1;
    }

    hal_spi_impl_t* impl = (hal_spi_impl_t*)bus->_impl;
    spi_bus_remove_device(impl->dev);
    spi_bus_free(impl->host);
    free(impl);
    bus->_impl = NULL;
    return 0;
}

/* Public: initialize a hal_spi_bus_t with the given function pointers */
void hal_spi_bus_init_struct(hal_spi_bus_t* bus)
{
    if (bus == NULL) return;
    bus->init = spi_init_impl;
    bus->write = spi_write_impl;
    bus->read = spi_read_impl;
    bus->deinit = spi_deinit_impl;
    bus->_impl = NULL;
}
