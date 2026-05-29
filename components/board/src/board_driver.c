#include "driver.h"

#include "board_devtable.h"

#include "esp_log.h"
#include <string.h>

static const char* kTag = "board_drv";

void board_register_all_drivers(void)
{
    /* Compile-time DTS mode: generated board_probe.c wires probe functions. */
}

int board_driver_probe_all(void)
{
    ESP_LOGI(kTag, "probing all devices from compile-time DTS table ...");
    int ok = 0, fail = 0;

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
         // 容错处理：设备存在，但没有找到匹配的驱动程序
        if (!probe) 
        {
            ESP_LOGW(kTag, "no generated probe for '%s' (compat=%s)",
                     device_get_name(dev), device_get_compatible(dev));
            device_set_status(dev, DEVICE_STATUS_DISABLED);// 标记为残废
            fail++;
            continue;
        }

        ESP_LOGI(kTag, "probing '%s' (%s) ...",
                 device_get_name(dev), device_get_compatible(dev));
        int ret = probe(dev);// 调用probe函数
        if (ret == 0) 
        {
            device_set_status(dev, DEVICE_STATUS_PROBED);
            ok++;
        } 
        else 
        {
            device_set_status(dev, DEVICE_STATUS_ERROR);
            fail++;
            ESP_LOGE(kTag, "probe FAILED: %s (ret=%d)", device_get_name(dev), ret);
        }
    }

    ESP_LOGI(kTag, "probe complete: %d ok, %d fail", ok, fail);
    return fail;
}

int board_driver_remove_all(void)
{
    ESP_LOGI(kTag, "removing all devices (reverse probe order) ...");

    const device_id_t* order = board_probe_order();
    int count = board_probe_order_count();

    /* 逆序 = children first, then parents */
    /* 2:关背光 -> 1:关屏幕 -> 0:关SPI*/
    for (int i = count - 1; i >= 0; i--)
    {
        device_id_t id = order[i];
        device_t* dev = board_dev_get(id);

        // 2. 核心拦截：只拆除那些【成功点亮】的设备
        if (!dev || device_get_status(dev) != DEVICE_STATUS_PROBED)
            continue;

        remove_fn_t remove_fn = board_remove_get_fn(id);
        if (remove_fn)
        {
            remove_fn(dev);
        }
        device_set_status(dev, DEVICE_STATUS_READY);
    }
    return 0;
}
