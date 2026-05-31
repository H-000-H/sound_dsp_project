#include "osal.h"
#include "board_config.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_task_wdt.h"

#include <stdarg.h>
#include <stdlib.h>

struct osal_mutex {
    SemaphoreHandle_t handle;
    StaticSemaphore_t sem_buf;  /* 仅静态创建时使用 */
};

/* 确认 OSAL_MUTEX_STORAGE_SIZE 足够容纳 */
_Static_assert(sizeof(struct osal_mutex) <= OSAL_MUTEX_STORAGE_SIZE,
               "OSAL_MUTEX_STORAGE_SIZE too small");

/* ── 上下文检测 (平台无关, 架构泄露防火墙)
 * FreeRTOS 提供 xPortInIsrContext() 覆盖所有架构 (Xtensa / RISC-V / ARM).
 * 框架层 (board_driver.c 等) 通过 osal_in_isr() 间接调用,
 * 不直接依赖 CMSIS __get_IPSR() 或 Xtensa 专有指令.
 */
int osal_in_isr(void)
{
    return (int)xPortInIsrContext();
}

/* ── Spinlock: 关中断临界区, 适配 FreeRTOS portMUX_TYPE ── */
struct osal_spinlock {
    portMUX_TYPE lock;
};

void osal_spinlock_init(osal_spinlock_t* lock)
{
    if (!lock) return;
    lock->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
}

void osal_spinlock_lock(osal_spinlock_t* lock)
{
    if (!lock) return;
    taskENTER_CRITICAL(&lock->lock);
}

void osal_spinlock_unlock(osal_spinlock_t* lock)
{
    if (!lock) return;
    taskEXIT_CRITICAL(&lock->lock);
}

/* ── 静态互斥锁池（禁止运行时动态分配） ── */


static struct osal_mutex s_mutex_pool[OSAL_MUTEX_POOL_SIZE];
static uint8_t s_mutex_used[OSAL_MUTEX_POOL_SIZE];
static portMUX_TYPE s_osal_pool_lock = portMUX_INITIALIZER_UNLOCKED;

int osal_pool_claim(volatile uint8_t* used_slots, size_t slot_count)
{
    if (!used_slots || slot_count == 0) return -1;

    int claimed_index = -1;
    taskENTER_CRITICAL(&s_osal_pool_lock);
    for (size_t i = 0; i < slot_count; i++) {
        if (!used_slots[i]) {
            used_slots[i] = 1;
            claimed_index = (int)i;
            break;
        }
    }
    taskEXIT_CRITICAL(&s_osal_pool_lock);
    return claimed_index;
}

void osal_pool_release(volatile uint8_t* used_slots, size_t slot_count, int slot_index)
{
    if (!used_slots || slot_index < 0 || (size_t)slot_index >= slot_count) return;
    taskENTER_CRITICAL(&s_osal_pool_lock);
    used_slots[slot_index] = 0;
    taskEXIT_CRITICAL(&s_osal_pool_lock);
}

uint32_t osal_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void osal_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t osal_ticks_from_ms(uint32_t ms)
{
    return pdMS_TO_TICKS(ms);
}

void* osal_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

void osal_free(void* ptr)
{
    free(ptr);
}

int osal_mutex_create(osal_mutex_t** out)
{
    if (!out) return -1;
    *out = NULL;

    int index = osal_pool_claim(s_mutex_used, OSAL_MUTEX_POOL_SIZE);
    if (index < 0) return -1;

    struct osal_mutex* m = &s_mutex_pool[index];
    m->handle = xSemaphoreCreateRecursiveMutexStatic(&m->sem_buf);
    if (!m->handle) {
        osal_pool_release(s_mutex_used, OSAL_MUTEX_POOL_SIZE, index);
        return -1;
    }
    *out = (osal_mutex_t*)m;
    return 0;
}

int osal_mutex_create_static(osal_mutex_t** out, void* storage, size_t storage_size)
{
    if (!out || !storage || storage_size < sizeof(struct osal_mutex)) return -1;
    *out = NULL;

    struct osal_mutex* m = (struct osal_mutex*)storage;
    m->handle = xSemaphoreCreateRecursiveMutexStatic(&m->sem_buf);
    if (!m->handle) return -1;

    *out = (osal_mutex_t*)m;
    return 0;
}

void osal_mutex_destroy(osal_mutex_t* mutex)
{
    if (!mutex) return;
    struct osal_mutex* m = (struct osal_mutex*)mutex;
    if (m->handle) {
        vSemaphoreDelete(m->handle);
        m->handle = NULL;
    }
    /* 如果是池中分配的，标记为未使用 */
    for (int i = 0; i < OSAL_MUTEX_POOL_SIZE; i++) {
        if (&s_mutex_pool[i] == m) {
            osal_pool_release(s_mutex_used, OSAL_MUTEX_POOL_SIZE, i);
            break;
        }
    }
}

int osal_mutex_lock(osal_mutex_t* mutex, uint32_t timeout_ms)
{
    if (!mutex || !mutex->handle) return -1;
    TickType_t ticks = (timeout_ms == OSAL_WAIT_FOREVER)
        ? portMAX_DELAY
        : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(mutex->handle, ticks) == pdTRUE ? 0 : -1;
}

int osal_mutex_unlock(osal_mutex_t* mutex)
{
    if (!mutex || !mutex->handle) return -1;
    return xSemaphoreGiveRecursive(mutex->handle) == pdTRUE ? 0 : -1;
}

int osal_task_create(const char* name, uint32_t stack_size,
                     uint32_t priority, osal_task_entry_t entry,
                     void* param, int core_id)
{
    TaskHandle_t handle = NULL;
    BaseType_t ret = xTaskCreatePinnedToCore(entry, name, stack_size,
                                             param, priority, &handle,
                                             (core_id >= 0) ? (BaseType_t)core_id : tskNO_AFFINITY);
    return (ret == pdPASS) ? 0 : -1;
}

/* ── 硬件安全关断 (weak, 板级必须覆写) ──
 * 默认实现: 触发硬故障 — 严禁静默通过.
 * 医疗/工业板级必须覆写此函数以执行:
 *   1. 关所有 PWM 输出
 *   2. 断开执行器/电机供电
 *   3. 拉低关键 GPIO
 *   4. 喂硬件看门狗, 等待系统复位
 *
 * 板级覆写后, 链接器自动选用强符号版本.
 */
__attribute__((weak)) void safety_hardware_shutdown(void)
{
    /*
     * 默认: 触发非法指令异常 (HardFault).
     * 板级若未覆写, PANIC 路径不会静默通过 — 系统必然停机.
     * 开发期间立即暴露缺失实现; 生产期间由强符号版本接管.
     */
    __asm__ volatile("ill");
}

/* ── Panic 安全互锁 (weak, 板级可覆盖) ──
 * 默认实现: 喂硬件看门狗让系统复位, 若 WDT 未启用则死循环.
 * 医疗/工业板级应覆盖此函数: 关 PWM → 切断执行器供电 → 喂狗.
 */
__attribute__((weak)) void osal_panic_interlock(void)
{
#if CONFIG_ESP_TASK_WDT_EN
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();
#endif
}

void osal_log(osal_log_level_t level, const char* tag, const char* fmt, ...)
{
    esp_log_level_t esp_level = ESP_LOG_INFO;
    switch (level) {
    case OSAL_LOG_ERROR: esp_level = ESP_LOG_ERROR; break;
    case OSAL_LOG_WARN:  esp_level = ESP_LOG_WARN; break;
    case OSAL_LOG_INFO:  esp_level = ESP_LOG_INFO; break;
    case OSAL_LOG_DEBUG: esp_level = ESP_LOG_DEBUG; break;
    default: break;
    }

    va_list args;
    va_start(args, fmt);
    esp_log_writev(esp_level, tag ? tag : "drv", fmt, args);
    va_end(args);
}

__attribute__((weak)) void production_log_push_fmt(prod_log_level_t level, const char* tag, const char* fmt, ...)
{
    (void)level;
    (void)tag;
    (void)fmt;
}
