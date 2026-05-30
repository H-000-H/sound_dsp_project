#include "driver.h"
#include "osal.h"

#include "board_devtable.h"

#include <string.h>

static const char* kTag = "board_drv";

static int device_has_failed_dependency(const device_t* dev)
{
    if (!dev || !dev->node || !dev->node->deps) return 0;

    for (int i = 0; i < dev->node->dep_count; i++) {
        device_t* dep = board_dev_get(dev->node->deps[i]);
        if (!dep) return 1;

        device_status_t status = device_get_status(dep);
        if (status == DEVICE_STATUS_DISABLED ||
            status == DEVICE_STATUS_ERROR ||
            status == DEVICE_STATUS_REMOVED) {
            return 1;
        }
    }
    return 0;
}

static void disable_dependents(device_id_t failed_id)
{
    for (int i = 0; i < board_dev_count(); i++) {
        device_t* child = board_dev_get((device_id_t)i);
        if (!child || !child->node || !child->node->deps) continue;

        for (int j = 0; j < child->node->dep_count; j++) {
            if (child->node->deps[j] == failed_id) {
                if (device_get_status(child) != DEVICE_STATUS_DISABLED) {
                    (void)device_set_status(child, DEVICE_STATUS_DISABLED);
                    DRV_LOGW(kTag, "disable dependent '%s' due to failed dependency", device_get_name(child));
                }
                break;
            }
        }
    }
}

void board_register_all_drivers(void)
{
    /* Compile-time DTS mode: generated board_probe.c wires probe functions. */
}

int board_driver_probe_all(void)
{
    DRV_LOGI(kTag, "probing all devices from compile-time DTS table ...");
    uint8_t ok = 0, fail = 0;

    const device_id_t* order = board_probe_order();
    int count = board_probe_order_count();

    for (int i = 0; i < count; i++) 
    {
        device_id_t id = order[i];
        device_t* dev = board_dev_get(id);
        probe_fn_t probe = board_probe_get_fn(id);// 获取设备对应的probe函数

        if (!dev || device_get_status(dev) == DEVICE_STATUS_DISABLED) {
            continue;
        }
        if (device_has_failed_dependency(dev)) {
            (void)device_set_status(dev, DEVICE_STATUS_DISABLED);
            fail++;
            DRV_LOGW(kTag, "skip '%s': dependency not available", device_get_name(dev));
            continue;
        }
         // 容错处理：设备存在，但没有找到匹配的驱动程序
        if (!probe) 
        {
            DRV_LOGW(kTag, "no generated probe for '%s' (compat=%s)",
                     device_get_name(dev), device_get_compatible(dev));
            (void)device_set_status(dev, DEVICE_STATUS_DISABLED);// 标记为残废
            disable_dependents(id);
            fail++;
            continue;
        }

        DRV_LOGI(kTag, "probing '%s' (%s) ...",
                 device_get_name(dev), device_get_compatible(dev));
        int ret = probe(dev);// 调用probe函数
        if (ret == 0) 
        {
            (void)device_set_status(dev, DEVICE_STATUS_PROBED);
            ok++;
        } 
        else 
        {
            (void)device_set_status(dev, DEVICE_STATUS_ERROR);
            disable_dependents(id);
            fail++;
            DRV_LOGE(kTag, "probe FAILED: %s (ret=%d)", device_get_name(dev), ret);
        }
    }

    DRV_LOGI(kTag, "probe complete: %d ok, %d fail", ok, fail);
    return fail;
}

int board_driver_remove_all(void)
{
    DRV_LOGI(kTag, "removing all devices (reverse probe order) ...");

    const device_id_t* order = board_probe_order();
    int count = board_probe_order_count();

    /* 逆序 = children first, then parents */
    /* 2:关背光 -> 1:关屏幕 -> 0:关SPI*/
    for (int i = count - 1; i >= 0; i--)
    {
        device_id_t id = order[i];
        device_t* dev = board_dev_get(id);

        // 2. 核心拦截：只拆除那些【成功点亮】的设备
        if (!dev)
            continue;

        device_status_t status = device_get_status(dev);
        if (status != DEVICE_STATUS_PROBED &&
            status != DEVICE_STATUS_RUNNING &&
            status != DEVICE_STATUS_SUSPENDED) {
            continue;
        }

        remove_fn_t remove_fn = board_remove_get_fn(id);
        if (remove_fn)
        {
            remove_fn(dev);
        }
        (void)device_set_status(dev, DEVICE_STATUS_READY);
    }
    return 0;
}
