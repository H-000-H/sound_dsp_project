#ifndef SENSOR_IF_H
#define SENSOR_IF_H

#include <stdint.h>
#include <stddef.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 传感器操作表 (驱动 probe 时注入 priv_data 魔术头) ── */
struct sensor_ops {
    int (*read_value)(device_t* dev, int* value, uint32_t timeout_ms);
};

#define SENSOR_IF_MAGIC 0x534E5352U  /* "SNSR" */

/* ── 传感器驱动 priv_data 魔术头 ──
 * 通过 device_set_subsys_priv() 显式绑定, 不依赖结构体偏移.
 * magic 做运行时类型鉴别, 避免在全局 device_t 中硬编码字段.
 */
typedef struct {
    uint32_t              magic;
    const struct sensor_ops* ops;
} sensor_if_priv_t;

/* ── 强类型传感器 API ──
 * 通过 dev->priv_data 中提取 ops 表分发.
 */
int sensor_read_value(device_t* dev, int* value, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_IF_H */
