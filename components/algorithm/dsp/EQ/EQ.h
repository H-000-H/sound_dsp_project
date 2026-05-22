#ifndef __EQ_H__
#define __EQ_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Q15 常量: 0.001f → (int32_t)(0.001 * 32768) ≈ 33 */
#define VOLUME_SMOOTH_ALPHA_Q15 33

/* High Shelf Filter — Q31 定点实现 */
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

/* 回调类型 (放在 filter_config_t 之后) */
typedef void (*eq_filter_callback)(filter_config_t *f, uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db);

/* 滤波器类型枚举 */
typedef enum 
{
    EQ_NONE=0,
    EQ_PEAK,
    EQ_HIGH_SHELF,
    EQ_LOW_SHELF,
    EQ_HIGH_PASS,
    EQ_LOW_PASS,
    EQ_BAND_PASS
}filter_classfiy;

/*一阶低通滤波 IIR控制音量*/
void set_audio_volume (float v);

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
    void (*process)(filter_config_t *f,const int32_t *in, int32_t *out, uint32_t len);
    void (*clear)(filter_config_t *f);
    void (*setset_filter)(filter_classfiy type, uint32_t f0, uint32_t Q, int32_t gain_db);
} EQ_Vtable;

typedef struct
{
    const EQ_Vtable * vptr;
    filter_config_t config;
    eq_filter_callback filter;
} EQ;

void set_filter(filter_classfiy type, uint32_t f0, uint32_t Q, int32_t gain_db);

extern const EQ_Vtable eq_vtable;
extern EQ eq_filter;
extern EQ* filter;

void process_audio_volume(int16_t* buffer, uint32_t sample_count);

typedef struct
{
    volatile bool flag_change;
    int32_t current_volume_q15;   /* Q15: 0 ~ 0x7FFF */
    int32_t target_volume_q15;    /* Q15: 0 = 静音, 0x7FFF = 1.0 */
    void (*set_audio_volume)(float volume);
    void (*process_audio_volume)(int16_t* buffer,uint32_t sample_count);
}volume_control;

extern volume_control volume_init;
extern volume_control* volume;
#ifdef __cplusplus
}
#endif

#endif /* __EQ_H__ */
