#ifndef __EQ_H__
#define __EQ_H__
#ifdef __cplusplus
extern "C"
{
#endif
#include "math.h"
#include <stdint.h>
/*============ 快速傅里叶变换（FFT）============*/
typedef struct
{
    uint32_t space;
    uint16_t fre;
    uint8_t  T;//幅度
} fft;
fft Cl_FFT(uint32_t*data,uint32_t len);
#ifdef __cplusplus
}
#endif
#endif