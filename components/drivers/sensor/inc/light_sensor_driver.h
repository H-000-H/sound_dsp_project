#ifndef __Light_Sensor__H
#define __Light_Sensor__H
#ifdef __cplusplus
extern "C"
{
#endif
/* ── ioctl 命令 ── */
#define LIGHT_SENSOR_CMD_READ 1  /* arg: int* — 输出光照值 0~100 */

#ifdef __cplusplus
}
#endif
#endif
