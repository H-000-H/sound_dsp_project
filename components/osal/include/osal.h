#ifndef BOARD_OSAL_H
#define BOARD_OSAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSAL_WAIT_FOREVER UINT32_MAX

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

/* ── Mutex ── */
#define OSAL_MUTEX_STORAGE_SIZE 96   /* 足够容纳 struct osal_mutex + 静态信号量缓存 */

int osal_mutex_create(osal_mutex_t** out);
int osal_mutex_create_static(osal_mutex_t** out, void* storage, size_t storage_size);
void osal_mutex_destroy(osal_mutex_t* mutex);
int osal_mutex_lock(osal_mutex_t* mutex, uint32_t timeout_ms);
int osal_mutex_unlock(osal_mutex_t* mutex);

/* ── Task ── */
int osal_task_create(const char* name, uint32_t stack_size,
                     uint32_t priority, osal_task_entry_t entry,
                     void* param, int core_id);

/* ── Logging ── */
void osal_log(osal_log_level_t level, const char* tag, const char* fmt, ...);

#define DRV_LOGE(tag, fmt, ...) osal_log(OSAL_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define DRV_LOGW(tag, fmt, ...) osal_log(OSAL_LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define DRV_LOGI(tag, fmt, ...) osal_log(OSAL_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define DRV_LOGD(tag, fmt, ...) osal_log(OSAL_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* BOARD_OSAL_H */
