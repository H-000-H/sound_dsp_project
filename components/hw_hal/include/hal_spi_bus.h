#ifndef HAL_SPI_BUS_H
#define HAL_SPI_BUS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_spi_bus hal_spi_bus_t;

typedef struct {
    int mosi;
    int miso;
    int sclk;
    int max_transfer_sz;
    int dma_chan;  /* -1 = auto */
} hal_spi_bus_config_t;

typedef struct {
    int mode;            /* 0-3 */
    int clock_speed_hz;
    int cs_pin;          /* -1 = none */
    int queue_size;
} hal_spi_device_config_t;

struct hal_spi_bus {
    int (*init)(hal_spi_bus_t* bus, const hal_spi_bus_config_t* bus_cfg,
                const hal_spi_device_config_t* dev_cfg);
    int (*write)(hal_spi_bus_t* bus, const uint8_t* data, size_t len);
    int (*read)(hal_spi_bus_t* bus, uint8_t* data, size_t len);
    int (*deinit)(hal_spi_bus_t* bus);
    void* _impl;
};

/* 使用默认函数指针初始化结构体（必须在调用 init 前调用） */
void hal_spi_bus_init_struct(hal_spi_bus_t* bus);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SPI_BUS_H */
