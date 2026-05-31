#include "light_sensor_driver.h"

#include "device.h"
#include "driver.h"
#include "VFS.h"
#include "vfs_adc.h"
#include "sensor.h"
#include "osal.h"
#include <string.h>
#include "board_config.h"

static const char* TAG = "gl5528";

#define LIGHT_SENSOR_ADC_TIMEOUT_MS  1000U
#define LIGHT_SENSOR_STOP_TIMEOUT_MS 100U

typedef struct
{
    sensor_if_priv_t sensor_hdr;    /* 子系统魔术头, 通过 subsys_priv 显式绑定 */
    device_t* adc_dev;
    int adc_channel;
    int adc_atten;
    int bitwidth;
    int interval_ms;
    int pool_idx;
} light_sensor_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
static light_sensor_priv_t s_light_sensor_pool[LIGHT_SENSOR_COUNT];
static uint8_t s_light_sensor_used[LIGHT_SENSOR_COUNT];

/* sensor_ops 回调声明 */
static int light_sensor_read_value(device_t* dev, int* value, uint32_t timeout_ms);

/* ── sensor_ops 表: 注入 priv_data 魔术头, 替代 ioctl/全局字段 ── */
static const struct sensor_ops light_sensor_sensor_ops = {
    .read_value = light_sensor_read_value,
};

/* VFS ops: ioctl 已废弃, 保留空表以供 VFS 生命周期 */
static int light_sensor_init(device_t* dev) { (void)dev; return 0; }
static const file_operation_t light_sensor_fops = {
    .init = light_sensor_init,
};

static int light_probe(device_t* dev)
{
    int adc_pin = -1, adc_channel = -1, adc_atten = -1, interval = 100;

    CRITICAL_ASSERT(device_get_prop_int(dev, "adc_pin", &adc_pin) == 0,
                    "Missing mandatory DTS property: adc_pin");
    CRITICAL_ASSERT(device_get_prop_int(dev, "adc_channel", &adc_channel) == 0,
                    "Missing mandatory DTS property: adc_channel");
    CRITICAL_ASSERT(device_get_prop_int(dev, "adc_atten", &adc_atten) == 0,
                    "Missing mandatory DTS property: adc_atten");

    device_get_prop_int(dev, "sample_interval_ms", &interval);

    int ret = 0;
    light_sensor_priv_t* priv = NULL;
    int pool_idx = -1;

    if (interval <= 0) {
        ret = VFS_ERR_INVAL;
        goto err_pool;
    }

    device_t* adc_dev = device_get_phandle_dev(dev, "adc");
    if (!adc_dev) {
        ret = VFS_ERR_DEFER;
        goto err_pool;
    }
    pool_idx = osal_pool_claim(s_light_sensor_used, LIGHT_SENSOR_COUNT);
    if (pool_idx < 0) {
        ret = VFS_ERR_NOMEM;
        goto err_pool;
    }
    priv = &s_light_sensor_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->pool_idx = pool_idx;
    priv->sensor_hdr.magic = SENSOR_IF_MAGIC;
    priv->sensor_hdr.ops   = &light_sensor_sensor_ops;
    priv->adc_dev = adc_dev;
    priv->adc_channel = adc_channel;
    priv->adc_atten = adc_atten;
    priv->bitwidth = 12;
    priv->interval_ms = interval;

    device_set_priv(dev, priv);
    device_set_subsys_priv(dev, &priv->sensor_hdr);
    dev->ops = &light_sensor_fops;
    DRV_LOGI(TAG, "probed: GPIO%d, chan=%d, atten=%d", adc_pin, adc_channel, adc_atten);
    return 0;

err_pool:
    if (pool_idx >= 0) osal_pool_release(s_light_sensor_used, LIGHT_SENSOR_COUNT, pool_idx);
    return ret;
}

static int light_remove(device_t* dev)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (priv) {
        device_ioctl(priv->adc_dev, ADC_CMD_STOP, NULL, 0, LIGHT_SENSOR_STOP_TIMEOUT_MS);
        osal_pool_release(s_light_sensor_used, LIGHT_SENSOR_COUNT, priv->pool_idx);
        device_ops_unregister(dev);
    }
    return 0;
}

DRIVER_REGISTER(light_sensor, "gl5528,photoresistor", light_probe, light_remove);

static int light_sensor_read_value(device_t* dev, int* value, uint32_t timeout_ms)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (!priv || !value) return VFS_ERR_INVAL;
    if (priv->sensor_hdr.magic != SENSOR_IF_MAGIC) return VFS_ERR_INVAL;

    int raw = 0;
    adc_read_arg_t read_arg = {
        .channel = priv->adc_channel,
        .atten = priv->adc_atten,
        .bitwidth = priv->bitwidth,
        .out_raw = &raw,
    };
    if (timeout_ms == 0) timeout_ms = LIGHT_SENSOR_ADC_TIMEOUT_MS;
    int ret = device_ioctl(priv->adc_dev, ADC_CMD_READ_RAW, &read_arg, sizeof(read_arg), timeout_ms);
    if (ret != 0) return ret;

    if (priv->bitwidth <= 0 || priv->bitwidth > 31) return VFS_ERR_INVAL;
    uint32_t max_raw = (1UL << (uint32_t)priv->bitwidth) - 1UL;
    if ((uint32_t)raw > max_raw) raw = (int)max_raw;
    if (raw < 0) raw = 0;
    *value = (int)(((uint32_t)raw * 100UL) / max_raw);
    return 0;
}
