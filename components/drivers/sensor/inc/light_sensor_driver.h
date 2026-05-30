#ifndef __Light_Sensor__H
#define __Light_Sensor__H
#ifdef __cplusplus
extern "C"
{
#endif
/* ── ioctl 命令 ── */
#define LIGHT_SENSOR_CMD_READ 1  /* arg: light_sensor_read_arg_t* — 输出光照值 0~100 */

/* ── ioctl arg 魔法数 (IEC 61508 类型安全) ── */
#define LIGHT_SENSOR_READ_MAGIC 0x52454144U  /* "READ" */

typedef struct {
    uint32_t magic;
    int value;
} light_sensor_read_arg_t;

#ifdef __cplusplus
}
#endif
#endif
