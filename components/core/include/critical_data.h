#pragma once

#include <cstdint>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 关键安全变量的双重反码存储 (IEC 60601-1 §14.6.1 / IEC 61508 §7.4.3.2)
 *
 * 应对 Brown-Out / 电压跌落 / 宇宙射线导致的 SRAM 位翻转.
 * 每个关键变量存储正码 + 反码两份副本, 每次读取自动校验.
 *
 * ⚠️ volatile 强制每次从物理 RAM 重读, 防止 GCC -O2/-Os 将
 *    if (speed != ~speed_inv) 优化删除 (编译器可静态证明此恒真).
 *
 * 用法:
 *   #include "critical_data.h"
 *   CRITICAL_VAR_DECL(int32_t, g_infusion_rate_ml_h);
 *
 *   CRITICAL_VAR_WRITE(g_infusion_rate_ml_h, 50);
 *
 *   int32_t rate;
 *   if (CRITICAL_VAR_READ(g_infusion_rate_ml_h, &rate)) {
 *       // 校验通过
 *   } else {
 *       enter_safe_state("CRITICAL_VAR corruption");
 *   }
 */

#define CRITICAL_VAR_DECL(type, name)  \
    volatile type name;                 \
    volatile type name##_inv

#define CRITICAL_VAR_WRITE(name, val)  \
    do {                                \
        (name) = (val);                 \
        (name##_inv) = ~(val);          \
    } while (0)

#define CRITICAL_VAR_READ(name, out)    \
    (((name) == ~(name##_inv))          \
     ? ((void)(*(out) = (name)), true)  \
     : (false))

#ifdef __cplusplus
}
#endif