#include "device.h"

#include "board_devtable.h"

#include <stdlib.h>
#include <string.h>

int device_tree_init(void)
{
    return board_dev_count() > 0 ? 0 : -1;
}

device_t* device_find(const char* name)
{
    device_id_t id = board_dev_find(name);
    if ((int)id < 0) return NULL;
    return (device_t*)board_dev_get(id);
}

device_t* device_find_by_compatible(const char* compatible)
{
    device_id_t id = board_dev_find_by_compat(compatible);
    if ((int)id < 0) return NULL;
    return (device_t*)board_dev_get(id);
}

device_t* device_get_parent(const device_t* dev)
{
    if (!dev || dev->dep_count <= 0 || !dev->deps) return NULL;
    return (device_t*)board_dev_get(dev->deps[0]);
}

int device_get_prop_int(const device_t* dev, const char* key, int* val)
{
    if (!dev || !key || !val) return -1;
    for (int i = 0; i < dev->prop_count; i++) {
        if (strcmp(dev->props[i].key, key) == 0) {
            *val = atoi(dev->props[i].value);
            return 0;
        }
    }
    return -1;
}

int device_get_prop_str(const device_t* dev, const char* key, const char** val)
{
    if (!dev || !key || !val) return -1;
    for (int i = 0; i < dev->prop_count; i++) {
        if (strcmp(dev->props[i].key, key) == 0) {
            *val = dev->props[i].value;
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
    return dev ? dev->name : NULL;
}

const char* device_get_compatible(const device_t* dev)
{
    return dev ? dev->compatible : NULL;
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

device_t* device_get_first(void)
{
    return board_dev_count() > 0 ? (device_t*)board_dev_get((device_id_t)0) : NULL;
}

device_t* device_get_next(const device_t* prev)
{
    if (!prev) return NULL;
    for (int i = 0; i < board_dev_count(); i++) {
        if (board_dev_get((device_id_t)i) == prev) {
            int next = i + 1;
            if (next >= board_dev_count()) return NULL;
            return (device_t*)board_dev_get((device_id_t)next);
        }
    }
    return NULL;
}

int device_get_count(void)
{
    return board_dev_count();
}
