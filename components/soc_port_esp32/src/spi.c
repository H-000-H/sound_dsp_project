#include "hal_spi_bus.h"

#include "driver/spi_master.h"
#include "osal.h"
#include "VFS.h"
#include <string.h>

static const char* kTag = "hal_spi_bus";

typedef struct {
    spi_host_device_t host;
    spi_device_handle_t dev;
    size_t max_transfer_sz;
    osal_mutex_t* lock;
} hal_spi_impl_t;

static spi_host_device_t spi_host_from_id(int host_id)
{
    if (host_id == 3) return SPI3_HOST;
    return SPI2_HOST;
}

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define SPI_IMPL_POOL_SIZE 2
static hal_spi_impl_t s_spi_pool[SPI_IMPL_POOL_SIZE];
static uint8_t s_spi_used[SPI_IMPL_POOL_SIZE];

static int spi_init_impl(hal_spi_bus_t* bus, const hal_spi_bus_config_t* bus_cfg,
                         const hal_spi_device_config_t* dev_cfg)
{
    if (bus == NULL || bus_cfg == NULL || dev_cfg == NULL)
    {
        return -1;
    }

    hal_spi_impl_t* impl = NULL;
    for (int i = 0; i < SPI_IMPL_POOL_SIZE; i++) {
        if (!s_spi_used[i]) {
            s_spi_used[i] = 1;
            impl = &s_spi_pool[i];
            memset(impl, 0, sizeof(*impl));
            break;
        }
    }
    if (!impl) {
        DRV_LOGE(kTag, "impl pool exhausted");
        return VFS_ERR_NOMEM;
    }

    int ret = 0;
    impl->host = spi_host_from_id(bus_cfg->host_id);
    impl->max_transfer_sz = (bus_cfg->max_transfer_sz > 0) ? (size_t)bus_cfg->max_transfer_sz : 4096U;
    if (osal_mutex_create(&impl->lock) != 0) {
        ret = -1;
        goto err_pool;
    }

    spi_bus_config_t esp_bus_cfg = {
        .mosi_io_num = bus_cfg->mosi,
        .miso_io_num = bus_cfg->miso,
        .sclk_io_num = bus_cfg->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)impl->max_transfer_sz,
    };

    spi_device_interface_config_t esp_dev_cfg = {
        .mode = dev_cfg->mode,
        .clock_speed_hz = dev_cfg->clock_speed_hz,
        .spics_io_num = dev_cfg->cs_pin,
        .queue_size = dev_cfg->queue_size > 0 ? dev_cfg->queue_size : 7,
    };

    esp_err_t esp_ret = spi_bus_initialize(impl->host, &esp_bus_cfg,
                                        (bus_cfg->dma_chan >= 0) ? bus_cfg->dma_chan : SPI_DMA_CH_AUTO);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "spi_bus_initialize failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_mutex;
    }

    esp_ret = spi_bus_add_device(impl->host, &esp_dev_cfg, &impl->dev);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(kTag, "spi_bus_add_device failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_bus;
    }

    bus->_impl = impl;
    return 0;

err_bus:
    spi_bus_free(impl->host);
err_mutex:
    osal_mutex_destroy(impl->lock);
err_pool:
    for (int i = 0; i < SPI_IMPL_POOL_SIZE; i++) {
        if (&s_spi_pool[i] == impl) { s_spi_used[i] = 0; break; }
    }
    return ret;
}

static int spi_write_impl(hal_spi_bus_t* bus, const uint8_t* data, size_t len)
{
    if (bus == NULL || bus->_impl == NULL || data == NULL || len == 0) {
        return -1;
    }

    hal_spi_impl_t* impl = (hal_spi_impl_t*)bus->_impl;

    if (len > impl->max_transfer_sz || len > (SIZE_MAX / 8U)) return -1;
    if (osal_mutex_lock(impl->lock, OSAL_WAIT_FOREVER) != 0) return -1;

    spi_transaction_t trans = {
        .length = len * 8U,
        .tx_buffer = data,
    };
    esp_err_t ret = spi_device_transmit(impl->dev, &trans);
    osal_mutex_unlock(impl->lock);
    if (ret != ESP_OK) {
        DRV_LOGE(kTag, "spi_device_transmit failed: %d", ret);
        return (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
    }
    return 0;
}

static int spi_read_impl(hal_spi_bus_t* bus, uint8_t* data, size_t len)
{
    if (bus == NULL || bus->_impl == NULL || data == NULL || len == 0) {
        return -1;
    }

    hal_spi_impl_t* impl = (hal_spi_impl_t*)bus->_impl;

    if (len > impl->max_transfer_sz || len > (SIZE_MAX / 8U)) return -1;
    if (osal_mutex_lock(impl->lock, OSAL_WAIT_FOREVER) != 0) return -1;

    spi_transaction_t trans = {
        .length = len * 8U,
        .rxlength = len * 8U,
        .rx_buffer = data,
    };
    esp_err_t ret = spi_device_transmit(impl->dev, &trans);
    osal_mutex_unlock(impl->lock);
    if (ret != ESP_OK) {
        DRV_LOGE(kTag, "spi_device_transmit failed: %d", ret);
        return (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
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
    osal_mutex_destroy(impl->lock);
    for (int i = 0; i < SPI_IMPL_POOL_SIZE; i++) { if (&s_spi_pool[i] == impl) { s_spi_used[i] = 0; break; } }
    bus->_impl = NULL;
    return 0;
}

void hal_spi_bus_init_struct(hal_spi_bus_t* bus)
{
    if (bus == NULL) return;
    bus->init = spi_init_impl;
    bus->write = spi_write_impl;
    bus->read = spi_read_impl;
    bus->deinit = spi_deinit_impl;
    bus->_impl = NULL;
}

#include "driver.h"

typedef struct {
    hal_spi_bus_t bus;
} spi_priv_t;

#define SPI_PRIV_POOL_SIZE 2
static spi_priv_t s_spi_priv_pool[SPI_PRIV_POOL_SIZE];
static uint8_t s_spi_priv_used[SPI_PRIV_POOL_SIZE];

static int spi_fops_write(device_t* dev, const void* buffer, size_t len)
{
    spi_priv_t* priv = (spi_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return priv->bus.write(&priv->bus, (const uint8_t*)buffer, len);
}

static int spi_fops_ioctl(device_t* dev, int cmd, void* arg)
{
    spi_priv_t* priv = (spi_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case SPI_CMD_READ: {
        if (!arg) return -1;
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
    int host = 2, mosi = -1, miso = -1, sclk = -1, dma = -1;
    int clk = 80000000, cs = -1, max_transfer = 4096;
    device_get_prop_int(dev, "host", &host);
    device_get_prop_int(dev, "mosi", &mosi);
    device_get_prop_int(dev, "miso", &miso);
    device_get_prop_int(dev, "sclk", &sclk);
    device_get_prop_int(dev, "dma_chan", &dma);
    device_get_prop_int(dev, "clock_speed_hz", &clk);
    device_get_prop_int(dev, "cs_pin", &cs);
    device_get_prop_int(dev, "max_transfer_sz", &max_transfer);

    if (mosi < 0 || sclk < 0 || clk <= 0 || max_transfer <= 0) {
        DRV_LOGE(kTag, "invalid SPI config");
        return -1;
    }

    spi_priv_t* priv = NULL;
    for (int i = 0; i < SPI_PRIV_POOL_SIZE; i++) {
        if (!s_spi_priv_used[i]) {
            s_spi_priv_used[i] = 1;
            priv = &s_spi_priv_pool[i];
            memset(priv, 0, sizeof(*priv));
            break;
        }
    }
    if (!priv) return VFS_ERR_NOMEM;

    hal_spi_bus_config_t bus_cfg = {
        .host_id = host,
        .mosi = mosi, .miso = miso, .sclk = sclk,
        .max_transfer_sz = max_transfer, .dma_chan = dma,
    };
    hal_spi_device_config_t dev_cfg = {
        .mode = 0, .clock_speed_hz = clk, .cs_pin = cs, .queue_size = 7,
    };

    hal_spi_bus_init_struct(&priv->bus);
    int ret = priv->bus.init(&priv->bus, &bus_cfg, &dev_cfg);
    if (ret != 0) {
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &spi_fops;
    DRV_LOGI(kTag, "SPI probed: host=%d MOSI=%d, MISO=%d, SCK=%d, clk=%d",
             host, mosi, miso, sclk, clk);
    return 0;

err_pool:
    for (int i = 0; i < SPI_PRIV_POOL_SIZE; i++) {
        if (&s_spi_priv_pool[i] == priv) { s_spi_priv_used[i] = 0; break; }
    }
    return ret;
}

static int spi_remove(device_t* dev)
{
    spi_priv_t* priv = (spi_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->bus.deinit(&priv->bus);
        for (int i = 0; i < SPI_PRIV_POOL_SIZE; i++) { if (&s_spi_priv_pool[i] == priv) { s_spi_priv_used[i] = 0; break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(spi, "esp32,spi-bus", spi_probe, spi_remove);
