#include "device.h"

#include "board_devtable.h"

#include <stdlib.h>
#include <string.h>

/* ── 运行时设备实例表 ── */
static device_t s_devices[DEV_ID_COUNT];

int device_tree_init(void)
{
    for (int i = 0; i < DEV_ID_COUNT; i++) {
        const device_node_t* node = board_node_get((device_id_t)i);
        s_devices[i].node      = node;
        s_devices[i].status    = node ? node->status : DEVICE_STATUS_DISABLED;
        s_devices[i].priv_data = NULL;
        s_devices[i].ops       = NULL;
    }
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
            *val = atoi(node->props[i].value);
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

int device_set_status(device_t* dev, device_status_t status)
{
    if (!dev) return -1;
    dev->status = status;
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
int device_open(device_t* dev, void* arg)
{
    if (!dev || !dev->ops || !dev->ops->open) return -1;
    return dev->ops->open(dev, arg);
}

int device_write(device_t* dev, const void* buf, size_t len)
{
    if (!dev || !dev->ops || !dev->ops->write) return -1;
    return dev->ops->write(dev, buf, len);
}

int device_read(device_t* dev, void* buf, size_t len)
{
    if (!dev || !dev->ops || !dev->ops->read) return -1;
    return dev->ops->read(dev, buf, len);
}

int device_ioctl(device_t* dev, int cmd, void* arg)
{
    if (!dev || !dev->ops || !dev->ops->ioctl) return -1;
    return dev->ops->ioctl(dev, cmd, arg);
}
