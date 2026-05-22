#ifndef BOARD_DEVICE_H
#define BOARD_DEVICE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 设备树常量 ── */
#define MAX_DEVICES   32

/* ── 设备节点 — 不透明类型 ── */
typedef struct device_node device_t;

/* ── 设备状态 ── */
typedef enum {
    DEVICE_STATUS_DISABLED = 0,
    DEVICE_STATUS_READY,
    DEVICE_STATUS_ERROR,
    DEVICE_STATUS_PROBED,
} device_status_t;

/* ── 查找设备 ── */
device_t* device_find(const char* name);
device_t* device_find_by_compatible(const char* compatible);
device_t* device_get_parent(const device_t* dev);

/* ── 读取属性 ── */
int device_get_prop_int(const device_t* dev, const char* key, int* val);
int device_get_prop_str(const device_t* dev, const char* key, const char** val);
int device_get_prop_bool(const device_t* dev, const char* key, int* val);
const char* device_get_name(const device_t* dev);
const char* device_get_compatible(const device_t* dev);
device_status_t device_get_status(const device_t* dev);

/* ── 设置设备状态（driver core 内部用） ── */
int device_set_status(device_t* dev, device_status_t status);

/* ── 设置设备私有数据（driver probe 时用） ── */
int device_set_priv(device_t* dev, void* priv);
void* device_get_priv(const device_t* dev);

/* ── 设备遍历 ── */
device_t* device_get_first(void);
device_t* device_get_next(const device_t* prev);
int device_get_count(void);

/* ── 设备树加载 ── */
int device_tree_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_DEVICE_H */
