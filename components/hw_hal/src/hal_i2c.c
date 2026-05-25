#include "hal_i2c.h"
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
                    