#include "EQ.h"
#include <math.h>
#include <stdint.h>

/* ── 内部工具 ────────────────────────────────────────────── */

/* 将 double 转为 Q(31-shift) 定点，带饱和 */
static int32_t double_to_q31(double val, int shift)
{
    if(shift>31) return 0;
    double scaled = val * (int64_t)(1u << (31 - shift));
    if (scaled >= 0x7FFFFFFF)  return 0x7FFFFFFF;
    if (scaled <= (int32_t)0x80000000) return (int32_t)0x80000000;
    return (int32_t)lrint(scaled);   /* lrint = round + 转 long */
}

/* 64 位累加器 → Q31 饱和裁剪 */
static int32_t sat_q31(int64_t v)
{
    if (v >  0x7FFFFFFF) return 0x7FFFFFFF;
    if (v < (int32_t)0x80000000) return (int32_t)0x80000000;
    return (int32_t)v;
}

/* ── API ────────────────────────────────────────────────── */
void Low_shelf_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db)
{
     /* ── 1. 浮点算原始 RBJ 系数 ── */
    double w0    = 2.0 * M_PI * f0 / fs;
    double cos_w = cos(w0);
    double sin_w = sin(w0);
    double A     = pow(10.0, gain_db / 40.0);
    double sqrtA = sqrt(A);
    double alpha = sin_w / (2.0 * Q);

    double b0 = A * ((A + 1) - (A - 1) * cos_w + 2 * sqrtA * alpha);
    double b1 = 2 * A * ((A - 1) - (A + 1) * cos_w);
    double b2 = A * ((A + 1) - (A - 1) * cos_w - 2 * sqrtA * alpha);
    double a0 = (A + 1) + (A - 1) * cos_w + 2 * sqrtA * alpha;
    double a1 = -2 * ((A - 1) + (A + 1) * cos_w);
    double a2 = (A + 1) + (A - 1) * cos_w - 2 * sqrtA * alpha;

    /* ── 2. 归一化: 除以 a0 ── */
    double c[5] = { b0 / a0, b1 / a0, b2 / a0,a1 / a0, a2 / a0 };

    /* ── 3. 确定移位量: 保证所有 |c[i]| * 2^(31-shift) ≤ INT32_MAX ── */
    double max_abs = 0.0;
    int i;
    for (i = 0; i < 5; i++) 
    {
        double v = fabs(c[i]);
        if (v > max_abs) max_abs = v;
    }

    int shift = 0;
    if (max_abs > 1.0)
        shift = (int)ceil(log2(max_abs));    /* 需要让出 shift 个整数位 */

    /* ── 4. 转 Q31 存储 ── */
    f->b0 = double_to_q31(c[0], shift);
    f->b1 = double_to_q31(c[1], shift);
    f->b2 = double_to_q31(c[2], shift);
    f->a1 = double_to_q31(c[3], shift);
    f->a2 = double_to_q31(c[4], shift);
    f->shift = shift;

    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;

    f->fs     = fs;
    f->f0     = f0;
    f->Q      = Q;
    f->gain_db = gain_db;
}

void high_shelf_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db)
{
    /* ── 1. 浮点算原始 RBJ 系数 ── */
    double w0    = 2.0 * M_PI * f0 / fs;
    double cos_w = cos(w0);
    double sin_w = sin(w0);
    double A     = pow(10.0, gain_db / 40.0);
    double sqrtA = sqrt(A);
    double alpha = sin_w / (2.0 * Q);

    double b0 = A * ((A + 1) + (A - 1) * cos_w + 2 * sqrtA * alpha);
    double b1 = -2 * A * ((A - 1) + (A + 1) * cos_w);
    double b2 = A * ((A + 1) + (A - 1) * cos_w - 2 * sqrtA * alpha);
    double a0 = (A + 1) - (A - 1) * cos_w + 2 * sqrtA * alpha;
    double a1 = 2 * ((A - 1) - (A + 1) * cos_w);
    double a2 = (A + 1) - (A - 1) * cos_w - 2 * sqrtA * alpha;

    /* ── 2. 归一化: 除以 a0 ── */
    double c[5] = { b0 / a0, b1 / a0, b2 / a0,a1 / a0, a2 / a0 };

    /* ── 3. 确定移位量: 保证所有 |c[i]| * 2^(31-shift) ≤ INT32_MAX ── */
    double max_abs = 0.0;
    int i;
    for (i = 0; i < 5; i++) 
    {
        double v = fabs(c[i]);
        if (v > max_abs) max_abs = v;
    }

    int shift = 0;
    if (max_abs > 1.0)
        shift = (int)ceil(log2(max_abs));    /* 需要让出 shift 个整数位 */

    /* ── 4. 转 Q31 存储 ── */
    f->b0 = double_to_q31(c[0], shift);
    f->b1 = double_to_q31(c[1], shift);
    f->b2 = double_to_q31(c[2], shift);
    f->a1 = double_to_q31(c[3], shift);
    f->a2 = double_to_q31(c[4], shift);
    f->shift = shift;

    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;

    f->fs     = fs;
    f->f0     = f0;
    f->Q      = Q;
    f->gain_db = gain_db;
}

void Peaking_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db)
{
     /* ── 1. 浮点算原始 RBJ 系数 ── */
    double w0    = 2.0 * M_PI * f0 / fs;
    double cos_w = cos(w0);
    double sin_w = sin(w0);
    double A     = pow(10.0, gain_db / 40.0);
    double alpha = sin_w / (2.0 * Q);

    double b0 = 1 + alpha * A;
    double b1 = -2 * cos_w;
    double b2 = 1 - alpha * A;
    double a0 = 1 + alpha / A ;
    double a1 = -2 * cos_w;
    double a2 = 1 - alpha/A;

    /* ── 2. 归一化: 除以 a0 ── */
    double c[5] = { b0 / a0, b1 / a0, b2 / a0,a1 / a0, a2 / a0 };

    /* ── 3. 确定移位量: 保证所有 |c[i]| * 2^(31-shift) ≤ INT32_MAX ── */
    double max_abs = 0.0;
    int i;
    for (i = 0; i < 5; i++) 
    {
        double v = fabs(c[i]);
        if (v > max_abs) max_abs = v;
    }

    int shift = 0;
    if (max_abs > 1.0)
        shift = (int)ceil(log2(max_abs));    /* 需要让出 shift 个整数位 */

    /* ── 4. 转 Q31 存储 ── */
    f->b0 = double_to_q31(c[0], shift);
    f->b1 = double_to_q31(c[1], shift);
    f->b2 = double_to_q31(c[2], shift);
    f->a1 = double_to_q31(c[3], shift);
    f->a2 = double_to_q31(c[4], shift);
    f->shift = shift;

    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;

    f->fs     = fs;
    f->f0     = f0;
    f->Q      = Q;
    f->gain_db = gain_db;
}

void Low_Pass_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db)
{
     /* ── 1. 浮点算原始 RBJ 系数 ── */
    double w0    = 2.0 * M_PI * f0 / fs;
    double cos_w = cos(w0);
    double sin_w = sin(w0);
    double alpha = sin_w / (2.0 * Q);

    double b0 = (1 - cos_w) / 2;
    double b1 = 1 - cos_w;
    double b2 = (1 - cos_w) / 2;
    double a0 = 1 + alpha ;
    double a1 = -2 * cos_w;
    double a2 = 1 - alpha;

    /* ── 2. 归一化: 除以 a0 ── */
    double c[5] = { b0 / a0, b1 / a0, b2 / a0,a1 / a0, a2 / a0 };

    /* ── 3. 确定移位量: 保证所有 |c[i]| * 2^(31-shift) ≤ INT32_MAX ── */
    double max_abs = 0.0;
    int i;
    for (i = 0; i < 5; i++) 
    {
        double v = fabs(c[i]);
        if (v > max_abs) max_abs = v;
    }

    int shift = 0;
    if (max_abs > 1.0)
        shift = (int)ceil(log2(max_abs));    /* 需要让出 shift 个整数位 */

    /* ── 4. 转 Q31 存储 ── */
    f->b0 = double_to_q31(c[0], shift);
    f->b1 = double_to_q31(c[1], shift);
    f->b2 = double_to_q31(c[2], shift);
    f->a1 = double_to_q31(c[3], shift);
    f->a2 = double_to_q31(c[4], shift);
    f->shift = shift;

    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;

    f->fs     = fs;
    f->f0     = f0;
    f->Q      = Q;
    f->gain_db = gain_db;
}

void High_Pass_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db)
{
     /* ── 1. 浮点算原始 RBJ 系数 ── */
    double w0    = 2.0 * M_PI * f0 / fs;
    double cos_w = cos(w0);
    double sin_w = sin(w0);
    double alpha = sin_w / (2.0 * Q);

    double b0 = (1 + cos_w) / 2;
    double b1 = - (1 + cos_w);
    double b2 = (1 + cos_w) / 2;
    double a0 = 1 + alpha ;
    double a1 = -2 * cos_w;
    double a2 = 1 - alpha;

    /* ── 2. 归一化: 除以 a0 ── */
    double c[5] = { b0 / a0, b1 / a0, b2 / a0,a1 / a0, a2 / a0 };

    /* ── 3. 确定移位量: 保证所有 |c[i]| * 2^(31-shift) ≤ INT32_MAX ── */
    double max_abs = 0.0;
    int i;
    for (i = 0; i < 5; i++) 
    {
        double v = fabs(c[i]);
        if (v > max_abs) max_abs = v;
    }

    int shift = 0;
    if (max_abs > 1.0)
        shift = (int)ceil(log2(max_abs));    /* 需要让出 shift 个整数位 */

    /* ── 4. 转 Q31 存储 ── */
    f->b0 = double_to_q31(c[0], shift);
    f->b1 = double_to_q31(c[1], shift);
    f->b2 = double_to_q31(c[2], shift);
    f->a1 = double_to_q31(c[3], shift);
    f->a2 = double_to_q31(c[4], shift);
    f->shift = shift;

    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;

    f->fs     = fs;
    f->f0     = f0;
    f->Q      = Q;
    f->gain_db = gain_db;
}

void Band_Pass_filter(filter_config_t *f,uint32_t fs, uint32_t f0, uint32_t Q, int32_t gain_db)
{
     /* ── 1. 浮点算原始 RBJ 系数 ── */
    double w0    = 2.0 * M_PI * f0 / fs;
    double cos_w = cos(w0);
    double sin_w = sin(w0);
    double alpha = sin_w / (2.0 * Q);

    double b0 = alpha;
    double b1 = 0;
    double b2 = -alpha;
    double a0 = 1 + alpha ;
    double a1 = -2 * cos_w;
    double a2 = 1 - alpha;

    /* ── 2. 归一化: 除以 a0 ── */
    double c[5] = { b0 / a0, b1 / a0, b2 / a0,a1 / a0, a2 / a0 };

    /* ── 3. 确定移位量: 保证所有 |c[i]| * 2^(31-shift) ≤ INT32_MAX ── */
    double max_abs = 0.0;
    int i;
    for (i = 0; i < 5; i++) 
    {
        double v = fabs(c[i]);
        if (v > max_abs) max_abs = v;
    }

    int shift = 0;
    if (max_abs > 1.0)
        shift = (int)ceil(log2(max_abs));    /* 需要让出 shift 个整数位 */

    /* ── 4. 转 Q31 存储 ── */
    f->b0 = double_to_q31(c[0], shift);
    f->b1 = double_to_q31(c[1], shift);
    f->b2 = double_to_q31(c[2], shift);
    f->a1 = double_to_q31(c[3], shift);
    f->a2 = double_to_q31(c[4], shift);
    f->shift = shift;

    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;

    f->fs     = fs;
    f->f0     = f0;
    f->Q      = Q;
    f->gain_db = gain_db;
}

void filter_process(filter_config_t *f,const int32_t *in, int32_t *out, uint32_t len)
{
    int32_t b0 = f->b0, b1 = f->b1, b2 = f->b2;
    int32_t a1 = f->a1, a2 = f->a2;
    int32_t x1 = f->x1, x2 = f->x2;
    int32_t y1 = f->y1, y2 = f->y2;
    int shift = f->shift;

    for (uint32_t i = 0; i < len; i++) 
    {
        int32_t x0 = in[i];

        /* y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2]
         *        - a1*y[n-1] - a2*y[n-2]
         *
         * 精度: 每个乘积 = Q31 × Q(31-shift) = Q(62-shift)
         * int64_t 累加 5 项安全 (完整推导见下)
         */
        int64_t acc = (int64_t)b0 * x0
                    + (int64_t)b1 * x1
                    + (int64_t)b2 * x2
                    - (int64_t)a1 * y1
                    - (int64_t)a2 * y2;

        /* 还原到 Q31: acc 里有 Q(62-shift), 右移 (31-shift) 得 Q31 */
        int32_t y0 = sat_q31(acc >> (31 - shift));

        out[i] = y0;

        /* 滚动状态 */
        x2 = x1;  x1 = x0;
        y2 = y1;  y1 = y0;
    }

    f->x1 = x1;  f->x2 = x2;
    f->y1 = y1;  f->y2 = y2;
}

void set_filter(filter_classfiy type, uint32_t f0, uint32_t Q, int32_t gain_db)
{
    uint32_t fs = filter->config.fs;
    switch(type) {
        case EQ_PEAK:       Peaking_filter(&filter->config, fs, f0, Q, gain_db); break;
        case EQ_HIGH_SHELF: high_shelf_filter(&filter->config, fs, f0, Q, gain_db); break;
        case EQ_LOW_SHELF:  Low_shelf_filter(&filter->config, fs, f0, Q, gain_db); break;
        case EQ_LOW_PASS:   Low_Pass_filter(&filter->config, fs, f0, Q, gain_db); break;
        case EQ_HIGH_PASS:  High_Pass_filter(&filter->config, fs, f0, Q, gain_db); break;
        case EQ_BAND_PASS:  Band_Pass_filter(&filter->config, fs, f0, Q, gain_db); break;
        default: return;
    }
}

void filter_clear(filter_config_t *f)
{
    f->x1 = f->x2 = 0;
    f->y1 = f->y2 = 0;
}

void set_audio_volume(float v)
{
    if(v < 0.0f) v = 0.0f;
    if(v > 1.0f) v = 1.0f;
    volume->target_volume_q15 = (int32_t)(v * 32768.0f);
    if(volume->target_volume_q15 > 0x7FFF) volume->target_volume_q15 = 0x7FFF;
    volume->flag_change = true;
}

void process_audio_volume(int16_t* buffer, uint32_t sample_count)
{
    int32_t cur = volume->current_volume_q15;
    int32_t tgt = volume->target_volume_q15;

    for(uint32_t i = 0; i < sample_count; i++)
    {
        /* 一阶平滑: cur += (tgt - cur) * ALPHA_Q15 / 32768 */
        int32_t diff = tgt - cur;
        cur += (diff * VOLUME_SMOOTH_ALPHA_Q15) >> 15;

        /* 应用音量: out = sample * cur / 32768 */
        int32_t sample = ((int32_t)buffer[i] * cur) >> 15;

        /* 饱和裁剪 */
        if(sample > INT16_MAX) sample = INT16_MAX;
        if(sample < INT16_MIN) sample = INT16_MIN;
        buffer[i] = (int16_t)sample;
    }

    if(cur == tgt) volume->flag_change = false;
    volume->current_volume_q15 = cur;
}

/* ── EQ.c 全局变量定义（声明在 EQ.h） ──── */
const EQ_Vtable eq_vtable =
{
    .process = filter_process,
    .clear  = filter_clear,
    .setset_filter = set_filter
};

EQ eq_filter =
{
    .vptr   = &eq_vtable,
    .config =
    {
        .b0    = 0x7FFFFFFF,
        .b1    = 0,
        .b2    = 0,
        .a1    = 0,
        .a2    = 0,
        .shift = 0,
        .x1    = 0,
        .x2    = 0,
        .y1    = 0,
        .y2    = 0,
        .fs    = 44100,
        .f0    = 1000,
        .Q     = 1,
        .gain_db = 0
    },
    .filter = NULL
};

EQ* filter = &eq_filter;

volume_control volume_init =
{
    .flag_change = false,
    .current_volume_q15 = 0x7FFF,
    .target_volume_q15  = 0x7FFF,
    .set_audio_volume = set_audio_volume,
    .process_audio_volume = process_audio_volume
};

volume_control* volume = &volume_init;