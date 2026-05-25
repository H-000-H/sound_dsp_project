#ifndef __Light_Sensor__H
#define __Light_Sensor__H
#include "device.h"
#ifdef __cplusplus
extern "C"
{
#endif
int light_sensor_read(device_t* dev,int* value);
#ifdef __cplusplus
}
#endif
#endif