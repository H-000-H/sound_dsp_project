#ifndef BOARD_DEVICE_H
#define BOARD_DEVICE_H

#include <stdint.h>
#include <stddef.h>

#include "board_nodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 设备树常量 ── */
#define MAX_DEVICES   DEV_ID_COUNT

/* ── 编译期属性: dtc-lite 在构建期展开, runtime 只读静态表 ── */
typedef struct
{
    const char* key;
    const char* value;
} device_prop_t;

/* ── 设备状态 ── */
typedef enum {
    DEVICE_STATUS_DISABLED = 0,
    DEVICE_STATUS_READY,
    DEVICE_STATUS_ERROR,
    DEVICE_STATUS_PROBED,
} device_status_t;

/* ── 前向声明 ── */
typedef struct device_instance device_t;

/* ── 编译期只读设备树节点 ── */
typedef struct device_node
{
    const char*         name;
    const char*         label;          /* DTS label (如 pwm_backlight) */
    const char*         compatible;
    const device_prop_t* props;
    const device_id_t*  deps;
    uint8_t             status;         /* 编译期默认状态 */
    uint8_t             prop_count;
    uint8_t             dep_count;
} device_node_t;


/* ── VFS 操作表 ── */
typedef struct file_operation
{
    int8_t (*init) (device_t* dev);
    int8_t (*open) (device_t* dev, void* arg);
    int8_t (*write)(device_t* dev, const void* buffer, size_t len);
    int8_t (*read) (device_t* dev, void* buffer, size_t len);
    int8_t (*ioctl)(device_t* dev, int cmd, void* arg);
} file_operation_t;

/* ── 运行时设备实例 ── */
typedef struct device_instance
{
    const device_node_t*   node;       /* 指向编译期节点 */
    device_status_t        status;     /* 运行时状态 */
    void*                  priv_data;  /* 驱动私有数据 */
    const file_operation_t* ops;       /* 操作函数表 */
} device_t;

/* ── 查找设备 ── */
device_t* device_find(const char* name);
device_t* device_find_by_label(const char* label);
device_t* device_find_by_compatible(const char* compatible);
device_t* device_get_parent(const device_t* dev);

/* ── 从属性中解析 phandle 引用并返回目标设备 ── */
device_t* device_get_phandle_dev(const device_t* dev, const char* key);

/* ── 读取属性（从 dev->node 读取） ── */
int device_get_prop_int(const device_t* dev, const char* key, int* val);
int device_get_prop_str(const device_t* dev, const char* key, const char** val);
int device_get_prop_bool(const device_t* dev, const char* key, int* val);
const char* device_get_name(const device_t* dev);
const char* device_get_compatible(const device_t* dev);
device_status_t device_get_status(const device_t* dev);

/* ── 运行时状态管理 ── */
int device_set_status(device_t* dev, device_status_t status);
int device_set_priv(device_t* dev, void* priv);
void* device_get_priv(const device_t* dev);

/* ── 设备遍历 ── */
device_t* device_get_first(void);
device_t* device_get_next(const device_t* prev);
int device_get_count(void);

/* ── 设备树加载 ── */
int device_tree_init(void);

/* ── VFS 便捷包装（空指针安全, 调用 dev->ops） ── */
int device_open(device_t* dev, void* arg);
int device_write(device_t* dev, const void* buf, size_t len);
int device_read(device_t* dev, void* buf, size_t len);
int device_ioctl(device_t* dev, int cmd, void* arg);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_DEVICE_H */
