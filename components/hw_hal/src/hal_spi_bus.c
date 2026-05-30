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

/* ===== SPI 平台驱动层 ===== */
#include "driver.h"

typedef struct {
    hal_spi_bus_t bus;
} spi_priv_t;

static int8_t spi_fops_write(device_t* dev, const void* buffer, size_t len)
{
    spi_priv_t* priv = (spi_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->bus.write(&priv->bus, (const uint8_t*)buffer, len);
}

static int8_t spi_fops_ioctl(device_t* dev, int cmd, void* arg)
{
    spi_priv_t* priv = (spi_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case SPI_CMD_READ: {
        spi_read_arg_t* a = (spi_read_arg_t*)arg;
        return priv->bus.read(&priv->bus, a->data, a->len);
    }
    case SPI_CMD_DEINIT:
        return priv->bus.deinit(&priv->bus);
    default:
        return -1;
    }
}

static const file_operation_t spi_fops = {
    .write = spi_fops_write,
    .ioctl = spi_fops_ioctl,
};

static int spi_probe(device_t* dev)
{
    int mosi, miso = -1, sclk, dma = -1, clk = 80000000, cs = -1;
    device_get_prop_int(dev, "mosi", &mosi);
    device_get_prop_int(dev, "miso", &miso);
    device_get_prop_int(dev, "sclk", &sclk);
    device_get_prop_int(dev, "dma_chan", &dma);
    device_get_prop_int(dev, "clock_speed_hz", &clk);
    device_get_prop_int(dev, "cs_pin", &cs);

    spi_priv_t* priv = (spi_priv_t*)calloc(1, sizeof(spi_priv_t));
    if (!priv) return -1;

    hal_spi_bus_config_t bus_cfg = {
        .mosi = mosi, .miso = miso, .sclk = sclk,
        .max_transfer_sz = 4096, .dma_chan = dma,
    };
    hal_spi_device_config_t dev_cfg = {
        .mode = 0, .clock_speed_hz = clk, .cs_pin = cs, .queue_size = 7,
    };

    hal_spi_bus_init_struct(&priv->bus);
    int ret = priv->bus.init(&priv->bus, &bus_cfg, &dev_cfg);
    if (ret != 0) { free(priv); return ret; }

    device_set_priv(dev, priv);
    dev->ops = &spi_fops;
    ESP_LOGI(kTag, "SPI probed: MOSI=%d, MISO=%d, SCK=%d, clk=%d", mosi, miso, sclk, clk);
    return 0;
}

static int spi_remove(device_t* dev)
{
    spi_priv_t* priv = (spi_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->bus.deinit(&priv->bus);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(spi, "esp32,spi-bus", spi_probe, spi_remove);
