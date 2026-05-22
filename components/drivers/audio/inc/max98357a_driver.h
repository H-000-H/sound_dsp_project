#ifndef MAX98357A_DRIVER_H
#define MAX98357A_DRIVER_H

#include <stdint.h>
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

int max98357a_init(device_t* dev);
int max98357a_set_enable(device_t* dev, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MAX98357A_DRIVER_H */
