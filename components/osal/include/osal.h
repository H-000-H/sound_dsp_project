#ifndef BOARD_OSAL_H
#define BOARD_OSAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSAL_WAIT_FOREVER UINT32_MAX
#define OSAL_LOCK_TIMEOUT_DEFAULT_MS 100U

typedef struct osal_mutex osal_mutex_t;
typedef void (*osal_task_entry_t)(void* param);

typedef enum {
    OSAL_LOG_ERROR = 0,
    OSAL_LOG_WARN,
    OSAL_LOG_INFO,
    OSAL_LOG_DEBUG,
} osal_log_level_t;

/* ── Time ── */
uint32_t osal_time_ms(void);
void osal_delay_ms(uint32_t ms);
uint32_t osal_ticks_from_ms(uint32_t ms);

/* ── Memory ── */
void* osal_calloc(size_t count, size_t size);
void osal_free(void* ptr);

/* ── 上下文检测 (平台无关, 架构泄露防火墙) ──
 * IEC 61508 §7.4.3.4: 框架层禁止出现 CPU 架构绑定指令.
 * 调用方仅依赖 osal.h, 实现由 osal_freertos.c 按平台适配:
 *   - FreeRTOS 全平台: xPortInIsrContext()
 *   - ARM 裸机: __get_IPSR()
 *   - 不允许在 board_driver.c 等框架层直接调用 CMSIS 汇编.
 */
int osal_in_isr(void);

/* ── Spinlock (临界区, 关中断保护) ── */
typedef struct osal_spinlock osal_spinlock_t;

#define OSAL_SPINLOCK_STORAGE_SIZE  8   /* 足够容纳 struct osal_spinlock + 对齐 */

void osal_spinlock_init(osal_spinlock_t* lock);
void osal_spinlock_lock(osal_spinlock_t* lock);
void osal_spinlock_unlock(osal_spinlock_t* lock);

/* ── Mutex ── */
#define OSAL_MUTEX_STORAGE_SIZE 96   /* 足够容纳 struct osal_mutex + 静态信号量缓存 */

int osal_mutex_create(osal_mutex_t** out);
int osal_mutex_create_static(osal_mutex_t** out, void* storage, size_t storage_size);
void osal_mutex_destroy(osal_mutex_t* mutex);
int osal_mutex_lock(osal_mutex_t* mutex, uint32_t timeout_ms);
int osal_mutex_unlock(osal_mutex_t* mutex);

/* ── Static Pool Helpers ── */
int osal_pool_claim(volatile uint8_t* used_slots, size_t slot_count);
void osal_pool_release(volatile uint8_t* used_slots, size_t slot_count, int slot_index);

/* ── Task ── */
int osal_task_create(const char* name, uint32_t stack_size,
                     uint32_t priority, osal_task_entry_t entry,
                     void* param, int core_id);

/* ── 安全互锁 (weak, 板级可重写, 保留用于 OEM 兼容) ── */
void osal_panic_interlock(void);

/* ── 硬件安全关断 (weak, 保留用于外部 OEM 代码兼容) ── */
void safety_hardware_shutdown(void);

/* ── 板级硬件安全关断 (强符号, 板级必须实现) ──
 * IEC 61508 §7.4.3.4 / IEC 62304 Class C:
 * 链接器强制检查 — 若 board_driver.c 未实现此函数, 链接失败.
 * 职责: portDISABLE_INTERRUPTS + hal_gpio_set_level_fast 拉低执行器 + hal_pwm_force_stop_all
 */
void system_safety_hardware_shutdown(const char* reason);

/* ── Panic (不可恢复错误, 工业/医疗 fail-fast → safe state) ──
 * 1. printf 输出致命原因
 * 2. 调用 system_safety_hardware_shutdown() — 强符号, 链接期强制检查
 * 3. 驻留死循环, 等待外部硬件看门狗复位
 * 永不返回.
 */
#undef OSAL_PANIC
#define OSAL_PANIC(fmt, ...) do { \
    printf("\r\n[FATAL ERROR] " fmt "\r\n", ##__VA_ARGS__); \
    system_safety_hardware_shutdown("OSAL_PANIC"); \
    while (1) { ; } \
} while (0)

/* ── Critical Assert (IEC 61508 §7.4.3.4: Fail-Fast) ──
 * 用于 probe/config 阶段的强制契约检查.
 * 如果 DTS 缺少强制属性或硬件配置不匹配, 必须立即停机, 严禁静默降级.
 */
#define CRITICAL_ASSERT(cond, fmt, ...) do { \
    if (!(cond)) { \
        printf("\r\n[CRITICAL_ASSERT FAILED] %s:%d: " fmt "\r\n", \
               __FILE__, __LINE__, ##__VA_ARGS__); \
        system_safety_hardware_shutdown("CRITICAL_ASSERT"); \
        while (1) { ; } \
    } \
} while (0)

/* ── Logging ── */
void osal_log(osal_log_level_t level, const char* tag, const char* fmt, ...);

#include "production_log.h"

#define DRV_LOGE(tag, fmt, ...) do { \
    osal_log(OSAL_LOG_ERROR, tag, fmt, ##__VA_ARGS__); \
    production_log_push_fmt(0, tag, fmt, ##__VA_ARGS__); \
} while(0)
#define DRV_LOGW(tag, fmt, ...) do { \
    osal_log(OSAL_LOG_WARN,  tag, fmt, ##__VA_ARGS__); \
    production_log_push_fmt(1, tag, fmt, ##__VA_ARGS__); \
} while(0)
#define DRV_LOGI(tag, fmt, ...) osal_log(OSAL_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define DRV_LOGD(tag, fmt, ...) osal_log(OSAL_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* BOARD_OSAL_H */
