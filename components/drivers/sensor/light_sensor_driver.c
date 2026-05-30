#include "light_sensor_driver.h"

#include "device.h"
#include "driver.h"
#include "vfs_adc.h"
#include "osal.h"
#include <string.h>

static const char* TAG = "gl5528";

typedef struct
{
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

static int light_sensor_ioctl(device_t* dev, int cmd, void* arg);
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

    if (adc_channel < 0 || interval <= 0) return -1;

    device_t* adc_dev = device_get_parent(dev);
    if (!adc_dev) return -1;

    light_sensor_priv_t* priv = NULL;
    for (int i = 0; i < LIGHT_SENSOR_PRIV_POOL_SIZE; i++) {
        if (!s_light_sensor_used[i]) {
            s_light_sensor_used[i] = 1;
            priv = &s_light_sensor_pool[i];
            memset(priv, 0, sizeof(*priv));
            break;
        }
    }
    if (!priv) return -1;
    priv->adc_dev = adc_dev;
    priv->adc_channel = adc_channel;
    priv->adc_atten = adc_atten;
    priv->bitwidth = 12;
    priv->interval_ms = interval;

    device_set_priv(dev, priv);
    dev->ops = &light_sensor_fops;
    DRV_LOGI(TAG, "probed: GPIO%d, chan=%d, atten=%d", adc_pin, adc_channel, adc_atten);
    return 0;
}

static int light_remove(device_t* dev)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (priv) {
        for (int i = 0; i < LIGHT_SENSOR_PRIV_POOL_SIZE; i++) { if (&s_light_sensor_pool[i] == priv) { s_light_sensor_used[i] = 0; break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(light_sensor, "gl5528,photoresistor", light_probe, light_remove);

static int light_sensor_read(device_t* dev, int* value)
{
    light_sensor_priv_t* priv = (light_sensor_priv_t*)device_get_priv(dev);
    if (!priv || !value) return -1;

    int raw = 0;
    adc_read_arg_t read_arg = {
        .channel = priv->adc_channel,
        .atten = priv->adc_atten,
        .bitwidth = priv->bitwidth,
        .out_raw = &raw,
    };
    int ret = device_ioctl(priv->adc_dev, ADC_CMD_READ_RAW, &read_arg);
    if (ret != 0) return ret;

    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    *value = (raw * 100) / 4095;
    return 0;
}

static int light_sensor_ioctl(device_t* dev, int cmd, void* arg)
{
    switch (cmd) {
    case LIGHT_SENSOR_CMD_READ:
        if (!arg) return -1;
        return light_sensor_read(dev, (int*)arg);
    default:
        return -1;
    }
}
