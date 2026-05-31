#pragma once

#include "esp_attr.h"

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

/*
 * Bootloop 退避防护 (SPI Flash 物理烧穿防御).
 *   safe_state_check_bootloop(): 每次异常启动时调用.
 *     连续 ≥ 5 次 → 永久安全锁死, 拒绝一切 Flash 写入.
 *   safe_state_clear_bootloop(): 正常冷启动/上电时调用, 计数器归零.
 */
bool safe_state_check_bootloop(void);
void safe_state_clear_bootloop(void);

/*
 * 进入不可恢复的安全状态 (IEC 61508 §7.4.3 / ISO 13485 §7.3.3)
 *
 * 本函数从 Task 上下文调用, 不能用于 NMI / ISR.
 * BOD NMI 等不可屏蔽中断请使用 safe_state_nmi_emergency_stamp().
 *
 * 本函数永不返回.
 */
void enter_safe_state(const char* reason) __attribute__((noreturn));

/*
 * BOD NMI 紧急标记 (IEC 61508 §7.4.3.2 掉电保护)
 * ─────────────────────────────────────
 * 必须在 IRAM 中, 仅执行寄存器级操作:
 *   1. 写 RTC_CNTL_STORE0_REG = 0xDEADBEEF (掉电标记, 冷启动后读取)
 *   2. 关所有 PWM 输出
 *   3. 拉高红色 LED (GPIO 直写寄存器)
 *
 * 严禁: printf / mutex / FreeRTOS API / Flash 访问
 *
 * 用法 (BOD NMI handler):
 *   void IRAM_ATTR brownout_isr(void* arg) {
 *       safe_state_nmi_emergency_stamp();
 *   }
 */
void IRAM_ATTR safe_state_nmi_emergency_stamp(void);

#ifdef __cplusplus
}
#endif