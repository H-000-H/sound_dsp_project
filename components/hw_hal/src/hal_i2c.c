#include "hal_i2c.h"
#include "driver.h"
#include "device.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <stdlib.h>

static const char* TAG = "hal_i2c";

/*hal层私有结构体*/
typedef struct
{
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dec_handle;/* 如果单设备可以放这里 */
}hal_i2c_impl_t;

static int i2c_init_impl(hal_i2c_bus_t*bus,const hal_i2c_config_t*cfg)
{
    if(!bus||!cfg)return -1;
    hal_i2c_impl_t* impl = calloc(1, sizeof(hal_i2c_impl_t));
    if(!impl) return -1;
    i2c_master_bus_config_t esp_cfg = 
    {
        .i2c_port = cfg->port,
        .sda_io_num = (gpio_num_t)cfg->sda_pin,
        .scl_io_num = (gpio_num_t)cfg->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&esp_cfg,&impl->bus_handle);
    if(ret!=ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %d", ret);
        free(impl);
        return ret;
    }
    bus->_impl = impl;
    return 0;
}

static int i2c_write_impl(hal_i2c_bus_t* bus,uint8_t addr,const uint8_t*data,size_t len ,uint32_t time_out)
{
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;
    i2c_master_transmit(impl->dec_handle,data,len,time_out);
    return 0 ;
}

static int i2c_read_impl(hal_i2c_bus_t* bus ,uint8_t addr, uint8_t*data , size_t len , uint32_t time_out)
{
    hal_i2c_impl_t* impl =(hal_i2c_impl_t*)bus->_impl;
    i2c_master_receive(impl->dec_handle,data,len,time_out);
    return 0;
}

static int i2c_read_write_imp(hal_i2c_bus_t* bus ,uint8_t addr,const uint8_t* wdata,size_t len ,
                    uint8_t* rdata, size_t rlen,uint32_t time_out)
{
    hal_i2c_impl_t* impl =(hal_i2c_impl_t*)bus->_impl;
    i2c_master_transmit(impl->dec_handle,wdata,len,time_out);
    i2c_master_receive(impl->dec_handle,rdata,rlen,time_out);
    return 0 ;
}

static int i2c_deinit(hal_i2c_bus_t*bus)
{
    if (!bus || !bus->_impl) return -1;
    hal_i2c_impl_t* impl = (hal_i2c_impl_t*)bus->_impl;

    if (impl->dec_handle) {
        i2c_master_bus_rm_device(impl->dec_handle);
        impl->dec_handle = NULL;
    }
    if (impl->bus_handle) {
        i2c_del_master_bus(impl->bus_handle);
        impl->bus_handle = NULL;
    }
    free(impl);
    bus->_impl = NULL;
    return 0;
}

void hal_i2c_init_struct(hal_i2c_bus_t* bus)
{
    if(!bus)return;
    bus->init = i2c_init_impl;
    bus->write = i2c_write_impl;
    bus->read = i2c_read_impl;
    bus->write_read = i2c_read_write_imp;
    bus->deinit = i2c_deinit;
    bus->_impl = NULL;
}

/* ===== I2C 平台驱动层 ===== */
typedef struct {
    hal_i2c_bus_t bus;
} i2c_priv_t;

static int8_t i2c_fops_ioctl(device_t* dev, int cmd, void* arg)
{
    i2c_priv_t* priv = (i2c_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    switch (cmd) {
    case I2C_CMD_WRITE: {
        i2c_rw_arg_t* a = (i2c_rw_arg_t*)arg;
        return priv->bus.write(&priv->bus, a->addr, a->data, a->len, a->timeout);
    }
    case I2C_CMD_READ: {
        i2c_rw_arg_t* a = (i2c_rw_arg_t*)arg;
        return priv->bus.read(&priv->bus, a->addr, a->data, a->len, a->timeout);
    }
    case I2C_CMD_WRITE_READ: {
        i2c_wr_arg_t* a = (i2c_wr_arg_t*)arg;
        return priv->bus.write_read(&priv->bus, a->addr, a->wdata, a->wlen, a->rdata, a->rlen, a->timeout);
    }
    case I2C_CMD_DEINIT:
        return priv->bus.deinit(&priv->bus);
    default:
        return -1;
    }
}

static const file_operation_t i2c_fops = {
    .ioctl = i2c_fops_ioctl,
};

static int i2c_probe(device_t* dev)
{
    int sda = -1, scl = -1, clock = 400000, port = 0;
    device_get_prop_int(dev, "sda_pin", &sda);
    device_get_prop_int(dev, "scl_pin", &scl);
    device_get_prop_int(dev, "clock_hz", &clock);
    device_get_prop_int(dev, "port", &port);

    if (sda < 0 || scl < 0) {
        ESP_LOGE(TAG, "missing I2C pin config");
        return -1;
    }

    i2c_priv_t* priv = (i2c_priv_t*)calloc(1, sizeof(i2c_priv_t));
    if (!priv) return -1;

    hal_i2c_config_t cfg = {
        .sda_pin = sda,
        .scl_pin = scl,
        .clock_hz = (uint32_t)clock,
        .port = port,
    };

    hal_i2c_init_struct(&priv->bus);
    int ret = priv->bus.init(&priv->bus, &cfg);
    if (ret != 0) { free(priv); return ret; }

    device_set_priv(dev, priv);
    dev->ops = &i2c_fops;
    ESP_LOGI(TAG, "I2C probed: SDA=%d, SCL=%d, clock=%d", sda, scl, clock);
    return 0;
}

static int i2c_remove(device_t* dev)
{
    i2c_priv_t* priv = (i2c_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->bus.deinit(&priv->bus);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(i2c, "esp32,i2c-bus", i2c_probe, i2c_remove);