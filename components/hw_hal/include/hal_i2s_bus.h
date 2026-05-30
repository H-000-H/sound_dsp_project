#ifndef HAL_I2S_BUS_H
#define HAL_I2S_BUS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_i2s_bus hal_i2s_bus_t;

typedef struct 
{
    int ws_pin;
    int bclk_pin;
    int dout_pin;
    int din_pin;         /* -1 = unused */
    int sample_rate;
    int bits_per_sample; /* 16 or 24 */
    int channel_format;  /* 0 = mono, 1 = stereo */
} hal_i2s_config_t;

struct hal_i2s_bus 
{
    int (*init)(hal_i2s_bus_t* bus, const hal_i2s_config_t* cfg);
    int (*write)(hal_i2s_bus_t* bus, const int16_t* samples, size_t bytes,
                 size_t* written, uint32_t timeout_ms);
    int (*deinit)(hal_i2s_bus_t* bus);
    void* _impl;
};

void hal_i2s_bus_init_struct(hal_i2s_bus_t* bus);

/* ── ioctl 命令 (平台驱动模式) ── */
#define I2S_CMD_WRITE       0x50  /* arg: i2s_write_arg_t* */
#define I2S_CMD_DEINIT      0x51  /* arg: NULL */

typedef struct {
    const int16_t* samples;
    size_t bytes;
    size_t* written;
    uint32_t timeout_ms;
} i2s_write_arg_t;

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2S_BUS_H */
