#ifndef __EQ_H__
#define __EQ_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
typedef void (*eq_filter_callback)(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

/* High Shelf Filter — Q31 定点实现 */
typedef enum
{
    Peak_filter=0,
    High_shelf_filter,
    Low_shelf_filter,
    High_pass_fiter,
    Low_pass_filter,
    Band_pass_filter
}filter_classfiy;

typedef struct 
{
    /* Q31 系数: 实际系数 = q31_val / 2^(31-shift) */
    int32_t b0, b1, b2;
    int32_t a1, a2;
    int8_t  shift;      /* 系数缩放，保证最大|系数| < 2^shift 时全放入 Q31 */

    /* 历史状态 (Q31) */
    int32_t x1, x2;     /* x[n-1], x[n-2] */
    int32_t y1, y2;     /* y[n-1], y[n-2] */

    /* 原始参数 (只读) */
    uint32_t fs;
    uint32_t f0;
    uint32_t Q;
    int32_t  gain_db;
} filter_config_t;
void Low_shelf_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

void Peaking_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

void Low_Pass_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

void High_Pass_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

void Band_Pass_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

void high_shelf_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);
/* 处理 len 个样本: in/out 可指向同一 buffer */
void filter_process(filter_config_t *f,const int32_t *in, int32_t *out, uint32_t len);
/* 清零历史状态 (静音/跳变后调用) */
void filter_clear(filter_config_t *f);

/* 原有回调结构 */
typedef struct 
{
    void (*EQ_driver)(eq_filter_callback filter_cb,filter_classfiy classfiy);
} EQ;

#ifdef __cplusplus
}
#endif

#endif /* __EQ_H__ */
