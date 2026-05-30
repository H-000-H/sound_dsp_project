#include "osal.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include <stdarg.h>
#include <stdlib.h>

struct osal_mutex {
    SemaphoreHandle_t handle;
    StaticSemaphore_t sem_buf;  /* 仅静态创建时使用 */
};

/* 确认 OSAL_MUTEX_STORAGE_SIZE 足够容纳 */
_Static_assert(sizeof(struct osal_mutex) <= OSAL_MUTEX_STORAGE_SIZE,
               "OSAL_MUTEX_STORAGE_SIZE too small");

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
#define OSAL_MUTEX_POOL_SIZE 24

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
