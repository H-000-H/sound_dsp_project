#pragma once
#include "esp_heap_caps.h"

/* Override LVGL internal memory pool to allocate from PSRAM */
static inline void * lvgl_psram_alloc(size_t size) {
    void * ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!ptr) {
        /* Fallback to DRAM if PSRAM fails */
        ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
    }
    return ptr;
}
