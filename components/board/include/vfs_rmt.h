#ifndef VFS_RMT_H
#define VFS_RMT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 与 hal_if/include/hal_rmt_led.h 类型相同，二者选一。 */
#ifndef HAL_RMT_LED_H

#define RMT_CMD_INIT         0x00
#define RMT_CMD_SET_RGB      0x01
#define RMT_CMD_SET_BRIGHT   0x02
#define RMT_CMD_OFF          0x03
#define RMT_CMD_DEINIT       0x04

typedef struct {
    int gpio;
    uint32_t resolution_hz;
} rmt_init_arg_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rmt_rgb_arg_t;

#endif /* ndef HAL_RMT_LED_H */

#ifdef __cplusplus
}
#endif

#endif /* VFS_RMT_H */
