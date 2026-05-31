#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ── 板级集中配置 (IEC 61508 §7.4.2.4: 单一配置源) ──
 *
 * 所有静态资源上限、池大小、硬件约束在此统一定义.
 * 驱动文件统一 #include "board_config.h" 获取所有宏,
 * 不得在各自的 .c 文件中魔数硬编码 POOL_SIZE.
 *
 * 来源:
 *   1. dt_config_gen.h  — DTC 编译期扫描 DTS 自动生成 (DTC_GEN_COUNT_xxx)
 *   2. 本文件            — 非 DTS 可推导的全局资源上限
 *
 * 移植到新平台/新产品时, 修改此文件即可, 无需逐个 .c 翻找.
 */

#include "dt_config_gen.h"

/* ═══════════════════════════════════════════════════════════════════
 *  DTS 自动推导的池大小 (由 dtc-lite.py 扫描 compatible 计数生成)
 *  ═══════════════════════════════════════════════════════════════════ */

/* SoC 外设总线 */
#define I2C_COUNT       DTC_GEN_COUNT_ESP32_I2C_BUS
#define SPI_COUNT       DTC_GEN_COUNT_ESP32_SPI_BUS
#define UART_COUNT      DTC_GEN_COUNT_ESP32_UART
#define ADC_COUNT       DTC_GEN_COUNT_ESP32_ADC
#define GPIO_COUNT      DTC_GEN_COUNT_ESP32_GPIO
#define I2S_COUNT       DTC_GEN_COUNT_ESP32_I2S_BUS

/* 设备驱动 */
#define LIGHT_SENSOR_COUNT  DTC_GEN_COUNT_GL5528_PHOTORESISTOR
#define ST7789_COUNT        DTC_GEN_COUNT_SITRONIX_ST7789
#define WS2812_COUNT        DTC_GEN_COUNT_WORLDSEMI_WS2812
#define MAX98357A_COUNT     DTC_GEN_COUNT_MAXIM_MAX98357A
#define GPIO_KEY_COUNT      DTC_GEN_COUNT_GPIO_KEYS
#define PWM_BACKLIGHT_COUNT DTC_GEN_COUNT_GENERIC_PWM_BACKLIGHT

/* ═══════════════════════════════════════════════════════════════════
 *  全局资源上限 (非 DTS 可推导, 需人工定义)
 *  ═══════════════════════════════════════════════════════════════════ */

/* ── 安全停机 ── */
#define BOARD_MAX_SAFETY_PINS     8
#define BOARD_SAFETY_MAX_CALLBACKS 4

/* ── OSAL 互斥锁池 (device_t per-device lock + 驱动内部锁) ── */
#define OSAL_MUTEX_POOL_SIZE      24

/* ── 设备树运行时 ── */
#define BOARD_MAX_DEVICE_COUNT    32

/* ── I2C 设备缓存 (per-bus 从设备地址缓存) ── */
#define I2C_DEV_CACHE_SIZE        4

/* ── PWM 硬件通道 ── */
#define PWM_MAX_CHANNELS          8

#endif /* BOARD_CONFIG_H */