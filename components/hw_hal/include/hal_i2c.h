#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明类型：用户只传指针，不关心内部 */
typedef struct hal_i2c_bus hal_i2c_bus_t;

typedef struct {
    int sda_pin;
    int scl_pin;
    uint32_t clock_hz;
    int port;
} hal_i2c_config_t;

/* 操作函数表 */
struct hal_i2c_bus 
{
    int (*init)(hal_i2c_bus_t* bus, const hal_i2c_config_t* cfg);
    int (*write)(hal_i2c_bus_t* bus, uint8_t addr, const uint8_t* data, size_t len, uint32_t time_out);
    int (*read)(hal_i2c_bus_t* bus, uint8_t addr, uint8_t* data, size_t len, uint32_t time_out);
    int (*write_read)(hal_i2c_bus_t* bus, uint8_t addr,
                      const uint8_t* wdata, size_t wlen,
                      uint8_t* rdata, size_t rlen, uint32_t time_out);
    int (*deinit)(hal_i2c_bus_t* bus);
    void* _impl;
};

void hal_i2c_init_struct(hal_i2c_bus_t* bus);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */