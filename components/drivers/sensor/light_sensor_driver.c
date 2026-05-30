#include "light_sensor_driver.h"

#include "device.h"
#include "driver.h"
#include "VFS.h"
#include "vfs_adc.h"
#include "osal.h"
#include <string.h>

static const char* TAG = "gl5528";

#define LIGHT_SENSOR_MAGIC 0x4C533332U  /* "LS32" */

typedef struct
{
    uint32_t magic;
    device_t* adc_dev;
    int adc_channel;
    int adc_atten;
    int bitwidth;
    int interval_ms;
} light_sensor_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define LIGHT_SENSOR_PRIV_POOL_SIZE 2
static light_sensor_priv_t s_light_sensor_pool[LIGHT_SENSOR_PRIV_POOL_SIZE];
static uint8_t s_light_sensor_used[LIGHT_SENSOR_PRIV_POOL_SIZE];

static int light_sensor_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len);
static const file_operation_t light_sensor_fops = {
    .ioctl = light_sensor_ioctl,
};

static int light_probe(device_t* dev)
{
    int adc_pin = -1, adc_channel = -1, adc_atten = 3, interval = 100;
    device_get_prop_int(dev, "adc_pin", &adc_pin);
    device_get_prop_int(dev, "adc_channel", &adc_channel);
    device_get_prop_int(dev, "adc_atten", &adc_atten);
    device_get_prop_int(dev, "sample_interval_ms", &interval);

    int ret = 0;
    light_sensor_priv_t* priv = NULL;

    if (adc_channel < 0 || interval <= 0) {
        ret = VFS_ERR_INVAL;
        goto err_pool;
    }

    device_t* adc_dev = device_get_parent(dev);
    if (!adc_dev) {
        ret = VFS_ERR_IO;
        goto err_pool;
    }
    int pool_idx = osal_pool_claim(s_light_sensor_used, LIGHT_SENSOR_PRIV_POOL_SIZE);
    if (pool_idx < 0) {
        ret = VFS_ERR_NOMEM;
        goto err_pool;
    }
    priv = &s_light_sensor_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));
    priv->magic = LIGHT_SENSOR_MAGIC;
    priv->adc_dev = adc_dev;
    priv->adc_channel = adc_channel;
    priv->adc_atten = adc_atten;
    priv->bitwidth = 12;
    priv->interval_ms = interval;

    device_set_priv(dev, priv);
    dev->ops = &light_sensor_fops;
    DRV_LOGI(TAG, "probed: GPIO%d, chan=%d, atten=%d", adc_pin, adc_channel, adc_atten);
    return 0;

err_pool:
    if (priv) {
        for (int i = 0; i < LIGHT_SENSOR_PRIV_POOL_SIZE; i++) {
            if (&s_light_sensor_pool[i] == priv) { osal_pool_release(s_light_sensor_used, LIGHT_SENSOR_PRIV_POOL_SIZE, i); break; }
        }
    }
    return ret;
}

static int light_remove(device_t* dev)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (priv) {
        if (priv->magic == LIGHT_SENSOR_MAGIC) {
            device_ioctl(priv->adc_dev, ADC_CMD_STOP, NULL, 0);
            priv->magic = 0;
        }
        for (int i = 0; i < LIGHT_SENSOR_PRIV_POOL_SIZE; i++) { if (&s_light_sensor_pool[i] == priv) { osal_pool_release(s_light_sensor_used, LIGHT_SENSOR_PRIV_POOL_SIZE, i); break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(light_sensor, "gl5528,photoresistor", light_probe, light_remove);

static int light_sensor_read(device_t* dev, int* value)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (!priv || priv->magic != LIGHT_SENSOR_MAGIC || !value) return VFS_ERR_INVAL;

    int raw = 0;
    adc_read_arg_t read_arg = {
        .channel = priv->adc_channel,
        .atten = priv->adc_atten,
        .bitwidth = priv->bitwidth,
        .out_raw = &raw,
    };
    int ret = device_ioctl(priv->adc_dev, ADC_CMD_READ_RAW, &read_arg, sizeof(read_arg));
    if (ret != 0) return ret;

    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    *value = (raw * 100) / 4095;
    return 0;
}

static int light_sensor_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (priv && priv->magic != LIGHT_SENSOR_MAGIC) return VFS_ERR_INVAL;

    switch (cmd) {
    case LIGHT_SENSOR_CMD_READ: {
        if (!arg || arg_len < sizeof(light_sensor_read_arg_t)) return VFS_ERR_INVAL;
        light_sensor_read_arg_t* a = (light_sensor_read_arg_t*)arg;
        if (a->magic != LIGHT_SENSOR_READ_MAGIC) return VFS_ERR_INVAL;
        return light_sensor_read(dev, &a->value);
    }
    default:
        return VFS_ERR_INVAL;
    }
}
