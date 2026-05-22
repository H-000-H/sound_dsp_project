#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_uart hal_uart_t;

typedef struct {
    int tx_pin;
    int rx_pin;
    int rts_pin;   /* -1 = unused */
    int cts_pin;   /* -1 = unused */
    int baud_rate;
    int data_bits; /* 5, 6, 7, 8 */
    int stop_bits; /* 1 or 2 */
    int parity;    /* 0 = none, 1 = odd, 2 = even */
} hal_uart_config_t;

struct hal_uart {
    int (*init)(hal_uart_t* uart, const hal_uart_config_t* cfg);
    int (*write)(hal_uart_t* uart, const uint8_t* data, size_t len);
    int (*read)(hal_uart_t* uart, uint8_t* data, size_t len, uint32_t timeout_ms);
    int (*deinit)(hal_uart_t* uart);
    void* _impl;
};

void hal_uart_init_struct(hal_uart_t* uart);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
