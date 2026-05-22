#include "driver.h"

#include "esp_log.h"
#include <string.h>

static const char* kTag = "board_drv";

#define MAX_DRIVERS      16

/* ── 静态驱动表 ── */
static board_driver_t* s_drivers[MAX_DRIVERS];
static int             s_driver_count = 0;

int board_driver_register(board_driver_t* drv)
{
    if (!drv || !drv->compatible || !drv->ops.probe)
    {
        ESP_LOGE(kTag, "invalid driver registration");
        return -1;
    }

    if (s_driver_count >= MAX_DRIVERS)
    {
        ESP_LOGE(kTag, "driver table full (%d)", MAX_DRIVERS);
        return -1;
    }

    for (int i = 0; i < s_driver_count; i++)
    {
        if (strcmp(s_drivers[i]->compatible, drv->compatible) == 0)
        {
            ESP_LOGW(kTag, "driver '%s' already registered", drv->compatible);
            return 0;
        }
    }

    s_drivers[s_driver_count++] = drv;
    ESP_LOGI(kTag, "registered driver: %s", drv->compatible);
    return 0;
}

static board_driver_t* find_driver(const char* compatible)
{
    if (!compatible) return NULL;
    for (int i = 0; i < s_driver_count; i++)
    {
        if (strcmp(s_drivers[i]->compatible, compatible) == 0)
            return s_drivers[i];
    }
    return NULL;
}

/* 递归 probe: parent → self */
static int probe_one(device_t* dev)
{
    if (!dev) return -1;

    device_status_t st = device_get_status(dev);
    if (st == DEVICE_STATUS_PROBED) return 0;
    if (st == DEVICE_STATUS_DISABLED) return -1;

    /* 先 probe parent */
    device_t* parent = device_get_parent(dev);
    if (parent)
    {
        int ret = probe_one(parent);
        if (ret != 0)
        {
            ESP_LOGE(kTag, "parent probe failed for '%s'", device_get_name(dev));
            device_set_status(dev, DEVICE_STATUS_ERROR);
            return ret;
        }
    }

    /* 找匹配 driver */
    const char* compat = device_get_compatible(dev);
    board_driver_t* drv = find_driver(compat);
    if (!drv)
    {
        ESP_LOGW(kTag, "no driver for '%s' (compat=%s)",
                 device_get_name(dev), compat ? compat : "NULL");
        device_set_status(dev, DEVICE_STATUS_DISABLED);
        return -1;
    }

    ESP_LOGI(kTag, "probing '%s' (%s) ...", device_get_name(dev), compat);

    int ret = drv->ops.probe(dev);
    if (ret == 0)
    {
        device_set_status(dev, DEVICE_STATUS_PROBED);
        ESP_LOGI(kTag, "probe OK: %s", device_get_name(dev));
    }
    else
    {
        device_set_status(dev, DEVICE_STATUS_ERROR);
        ESP_LOGE(kTag, "probe FAILED: %s (ret=%d)", device_get_name(dev), ret);
    }
    return ret;
}

int board_driver_probe_all(void)
{
    ESP_LOGI(kTag, "probing all devices ...");
    int ok = 0, fail = 0;

    device_t* dev = device_get_first();
    while (dev)
    {
        if (probe_one(dev) == 0)
            ok++;
        else
            fail++;
        dev = device_get_next(dev);
    }

    ESP_LOGI(kTag, "probe complete: %d ok, %d fail", ok, fail);
    return fail;
}

int board_driver_remove_all(void)
{
    ESP_LOGI(kTag, "removing all devices (reverse order) ...");

    /* 反向遍历, 先 remove children, 再 parent.
     * 简单实现: 先收集所有 probed device, 然后反向 probe. */
    device_t* list[MAX_DEVICES];
    int count = 0;

    device_t* dev = device_get_first();
    while (dev && count < MAX_DEVICES)
    {
        if (device_get_status(dev) == DEVICE_STATUS_PROBED)
            list[count++] = dev;
        dev = device_get_next(dev);
    }

    for (int i = count - 1; i >= 0; i--)
    {
        const char* compat = device_get_compatible(list[i]);
        board_driver_t* drv = find_driver(compat);
        if (drv && drv->ops.remove)
        {
            drv->ops.remove(list[i]);
        }
        device_set_status(list[i], DEVICE_STATUS_READY);
    }
    return 0;
}
