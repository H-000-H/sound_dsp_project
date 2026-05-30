#include "device.h"
#include "VFS.h"
#include "osal.h"

#include "board_devtable.h"

#include <stdlib.h>
#include <string.h>

/* ── 运行时设备实例表 ── */
static device_t s_devices[DEV_ID_COUNT];
static uint8_t s_device_lock_storage[DEV_ID_COUNT][OSAL_MUTEX_STORAGE_SIZE];

/* ── board 层静态数据缓冲区 (platform_data 注入) ── */
static uint8_t s_ws2812_rgb_buf[3];

/* ── device_set_status FSM 原子锁 (IEC 61508 2.7.1) ── */
static osal_spinlock_t s_status_lock;

static void board_inject_platform_data(void)
{
    /* WS2812: 注入 RGB 缓冲区, 驱动通过 dev->platform_data 获取 */
    device_t* led_dev = board_dev_get(DEV_ID_LED_0);
    if (led_dev) {
        memset(s_ws2812_rgb_buf, 0, sizeof(s_ws2812_rgb_buf));
        led_dev->platform_data = s_ws2812_rgb_buf;
    }
}

static int device_status_can_transit(device_status_t from, device_status_t to)
{
    if (from == to) return 1;

    switch (from) {
    case DEVICE_STATUS_DISABLED:
        return to == DEVICE_STATUS_READY || to == DEVICE_STATUS_UNINIT;
    case DEVICE_STATUS_UNINIT:
        return to == DEVICE_STATUS_READY || to == DEVICE_STATUS_ERROR || to == DEVICE_STATUS_DISABLED;
    case DEVICE_STATUS_READY:
        return to == DEVICE_STATUS_PROBED || to == DEVICE_STATUS_DISABLED || to == DEVICE_STATUS_ERROR;
    case DEVICE_STATUS_PROBED:
        return to == DEVICE_STATUS_RUNNING || to == DEVICE_STATUS_SUSPENDED ||
               to == DEVICE_STATUS_READY || to == DEVICE_STATUS_REMOVED || to == DEVICE_STATUS_ERROR;
    case DEVICE_STATUS_RUNNING:
        return to == DEVICE_STATUS_SUSPENDED || to == DEVICE_STATUS_READY ||
               to == DEVICE_STATUS_REMOVED || to == DEVICE_STATUS_ERROR;
    case DEVICE_STATUS_SUSPENDED:
        return to == DEVICE_STATUS_RUNNING || to == DEVICE_STATUS_READY ||
               to == DEVICE_STATUS_REMOVED || to == DEVICE_STATUS_ERROR;
    case DEVICE_STATUS_ERROR:
        return 0;
    case DEVICE_STATUS_REMOVED:
        return to == DEVICE_STATUS_READY || to == DEVICE_STATUS_DISABLED;
    default:
        return 0;
    }
}

int device_tree_init(void)
{
    for (int i = 0; i < DEV_ID_COUNT; i++) {
        const device_node_t* node = board_node_get((device_id_t)i);
        s_devices[i].node      = node;
        s_devices[i].status    = node ? node->status : DEVICE_STATUS_DISABLED;
        s_devices[i].priv_data = NULL;
        s_devices[i].ops       = NULL;
        s_devices[i].lock      = NULL;
        s_devices[i].platform_data = NULL;

        if (node && s_devices[i].status != DEVICE_STATUS_DISABLED) {
            osal_mutex_t* lock = NULL;
            if (osal_mutex_create_static(&lock, s_device_lock_storage[i], sizeof(s_device_lock_storage[i])) == 0) {
                s_devices[i].lock = lock;
            } else {
                OSAL_PANIC("device_tree_init: mutex create failed for dev %d", i);
            }
        }
    }
    osal_spinlock_init(&s_status_lock);
    board_inject_platform_data();
    return board_dev_count() > 0 ? 0 : -1;
}




/* ── 运行时设备实例访问 ── */
device_t* board_dev_get(device_id_t id)
{
    if ((int)id < 0 || (int)id >= DEV_ID_COUNT) return NULL;
    return &s_devices[id];
}

device_t* device_find(const char* name)
{
    device_id_t id = board_dev_find(name);
    if ((int)id < 0) return NULL;
    return board_dev_get(id);
}

device_t* device_find_by_label(const char* label)
{
    device_id_t id = board_dev_find_by_label(label);
    if ((int)id < 0) return NULL;
    return board_dev_get(id);
}

device_t* device_get_phandle_dev(const device_t* dev, const char* key)
{
    const char* val;
    if (device_get_prop_str(dev, key, &val) != 0) return NULL;
    /* dtc-lite 将 phandle 引用存为 label 名字符串 */
    return device_find_by_label(val);
}

device_t* device_find_by_id(device_id_t id)
{
    return board_dev_get(id);
}

device_t* device_find_by_path(const char* path)
{
    if (!path) return NULL;
    for (int i = 0; i < DEV_ID_COUNT; i++) {
        const device_node_t* node = board_node_get((device_id_t)i);
        if (node && node->path && strcmp(node->path, path) == 0)
            return board_dev_get((device_id_t)i);
    }
    return NULL;
}

device_t* device_find_by_compatible(const char* compatible)
{
    device_id_t id = board_dev_find_by_compat(compatible);
    if ((int)id < 0) return NULL;
    return board_dev_get(id);
}

device_t* device_get_parent(const device_t* dev)
{
    if (!dev || !dev->node) return NULL;
    const device_node_t* node = dev->node;
    if (node->dep_count <= 0 || !node->deps) return NULL;
    return board_dev_get(node->deps[0]);
}

/* ── 属性读取（通过 dev->node） ── */
int device_get_prop_int(const device_t* dev, const char* key, int* val)
{
    if (!dev || !dev->node || !key || !val) return -1;
    const device_node_t* node = dev->node;
    for (int i = 0; i < node->prop_count; i++) {
        if (strcmp(node->props[i].key, key) == 0)
        {
            char* end = NULL;
            long parsed = strtol(node->props[i].value, &end, 0);
            if (!end || *end != '\0') return -1;
            if (parsed < INT32_MIN || parsed > INT32_MAX) return -1;
            *val = (int)parsed;
            return 0;
        }
    }
    return -1;
}

int device_get_prop_str(const device_t* dev, const char* key, const char** val)
{
    if (!dev || !dev->node || !key || !val) return -1;
    const device_node_t* node = dev->node;
    for (int i = 0; i < node->prop_count; i++) {
        if (strcmp(node->props[i].key, key) == 0) {
            *val = node->props[i].value;
            return 0;
        }
    }
    return -1;
}

int device_get_prop_bool(const device_t* dev, const char* key, int* val)
{
    return device_get_prop_int(dev, key, val);
}

const char* device_get_name(const device_t* dev)
{
    return dev && dev->node ? dev->node->name : NULL;
}

const char* device_get_compatible(const device_t* dev)
{
    return dev && dev->node ? dev->node->compatible : NULL;
}

device_status_t device_get_status(const device_t* dev)
{
    return dev ? dev->status : DEVICE_STATUS_DISABLED;
}

device_criticality_t device_get_criticality(const device_t* dev)
{
    if (!dev || !dev->node) return DEVICE_CRIT_WARNING;
    return (device_criticality_t)dev->node->criticality;
}

int device_set_status(device_t* dev, device_status_t status)
{
    if (!dev) return -1;
    osal_spinlock_lock(&s_status_lock);
    if (!device_status_can_transit(dev->status, status)) {
        osal_spinlock_unlock(&s_status_lock);
        return -1;
    }
    dev->status = status;
    osal_spinlock_unlock(&s_status_lock);
    return 0;
}

int device_set_priv(device_t* dev, void* priv)
{
    if (!dev) return -1;
    dev->priv_data = priv;
    return 0;
}

void* device_get_priv(const device_t* dev)
{
    return dev ? dev->priv_data : NULL;
}

/* ── 设备遍历 ── */
device_t* device_get_first(void)
{
    return board_dev_count() > 0 ? board_dev_get((device_id_t)0) : NULL;
}

device_t* device_get_next(const device_t* prev)
{
    if (!prev) return NULL;
    for (int i = 0; i < board_dev_count(); i++)
    {
        if (board_dev_get((device_id_t)i) == prev)
        {
            int next = i + 1;
            if (next >= board_dev_count()) return NULL;
            return board_dev_get((device_id_t)next);
        }
    }
    return NULL;
}

int device_get_count(void)
{
    return board_dev_count();
}

/* ── VFS 便捷包装 ── */
static int device_can_access(const device_t* dev)
{
    if (!dev || !dev->ops) return -1;
    /* IEC 62304 Class C: 仅 RUNNING 态允许 I/O (禁用 PROBED 态幽灵访问) */
    if (dev->status == DEVICE_STATUS_RUNNING) return 0;
    return -1;
}

/* ── 内部锁辅助: 自动抓锁 + 访问校验, 返回 0 可继续, 否则已解锁 ── */
static int device_lock_and_check(device_t* dev)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;
    if (device_can_access(dev) != 0) {
        device_unlock(dev);
        return -1;
    }
    return 0;
}

int device_open(device_t* dev, void* arg)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;

    if (!dev->ops || (!dev->ops->open && !dev->ops->init)) {
        device_unlock(dev);
        return -1;
    }
    if (dev->status != DEVICE_STATUS_PROBED) {
        device_unlock(dev);
        return -1;
    }

    ret = dev->ops->open ? dev->ops->open(dev, arg) : dev->ops->init(dev);
    if (ret == 0) {
        device_set_status(dev, DEVICE_STATUS_RUNNING);
    }
    device_unlock(dev);
    return ret;
}

int device_close(device_t* dev)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;

    if (dev->status != DEVICE_STATUS_RUNNING) {
        device_unlock(dev);
        return -1;
    }
    if (dev->ops && dev->ops->close) {
        ret = dev->ops->close(dev);
        if (ret != 0) {
            device_unlock(dev);
            return ret;
        }
    }
    device_set_status(dev, DEVICE_STATUS_PROBED);
    device_unlock(dev);
    return 0;
}

int device_write(device_t* dev, const void* buf, size_t len, uint32_t timeout_ms)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;
    if (device_can_access(dev) != 0 || !dev->ops->write) {
        device_unlock(dev);
        return -1;
    }
    ret = dev->ops->write(dev, buf, len, timeout_ms);
    device_unlock(dev);
    return ret;
}

int device_read(device_t* dev, void* buf, size_t len, uint32_t timeout_ms)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;
    if (device_can_access(dev) != 0 || !dev->ops->read) {
        device_unlock(dev);
        return -1;
    }
    ret = dev->ops->read(dev, buf, len, timeout_ms);
    device_unlock(dev);
    return ret;
}

int device_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;
    if (device_can_access(dev) != 0 || !dev->ops->ioctl) {
        device_unlock(dev);
        return -1;
    }
    ret = dev->ops->ioctl(dev, cmd, arg, arg_len);
    device_unlock(dev);
    return ret;
}

int device_suspend(device_t* dev)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;

    if (dev->status != DEVICE_STATUS_RUNNING) {
        device_unlock(dev);
        return -1;
    }
    if (dev->ops && dev->ops->suspend) {
        ret = dev->ops->suspend(dev);
        if (ret != 0) {
            device_unlock(dev);
            return ret;
        }
    }
    device_set_status(dev, DEVICE_STATUS_SUSPENDED);
    device_unlock(dev);
    return 0;
}

int device_resume(device_t* dev)
{
    if (!dev) return -1;
    int ret = device_lock(dev);
    if (ret != 0) return ret;

    if (dev->status != DEVICE_STATUS_SUSPENDED) {
        device_unlock(dev);
        return -1;
    }
    if (dev->ops && dev->ops->resume) {
        ret = dev->ops->resume(dev);
        if (ret != 0) {
            device_unlock(dev);
            return ret;
        }
    }
    device_set_status(dev, DEVICE_STATUS_RUNNING);
    device_unlock(dev);
    return 0;
}

/* ── 设备锁（启动期静态创建，运行期仅有限时加锁） ── */
int device_lock(device_t* dev)
{
    if (!dev) return -1;
    if (!dev->lock) return VFS_ERR_BUSY;
    return osal_mutex_lock(dev->lock, OSAL_LOCK_TIMEOUT_DEFAULT_MS) == 0 ? VFS_OK : VFS_ERR_BUSY;
}

int device_unlock(device_t* dev)
{
    if (!dev || !dev->lock) return -1;
    return osal_mutex_unlock(dev->lock) == 0 ? VFS_OK : VFS_ERR_IO;
}
