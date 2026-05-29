#include "light_sensor_driver.h"
#include "device.h"
#include "driver.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "stdlib.h"

static const char* TAG = "gl5528";
typedef struct
{
    adc_oneshot_unit_handle_t adc_handle;
    int adc_chanel;
    int interval_ms;
}light_sensor_priv_t;

/* ── VFS 操作表（无 init, probe 自带初始化） ── */
static int8_t light_sensor_ioctl(device_t* dev, int cmd, void* arg);
static const file_operation_t light_sensor_fops = {
    .init  = NULL,
    .ioctl = light_sensor_ioctl,
};

static int light_probe(device_t* dev)
{
    int adc_pin = 1, adc_channel = 0, adc_atten = 3, interval = 100;

    // 读取属性
    device_get_prop_int(dev,"adc_pin",&adc_pin);
    device_get_prop_int(dev,"adc_channel",&adc_channel);
    device_get_prop_int(dev,"adc_atten",&adc_atten);
    device_get_prop_int(dev,"sample_interval_ms",&interval);

    //初始化ADC
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t unit_cfg =
    {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&unit_cfg,&adc_handle);

    adc_oneshot_chan_cfg_t  chan_cfg=
    {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12 ,// 12位宽
    };

    adc_oneshot_config_channel(adc_handle,adc_channel,&chan_cfg);

    light_sensor_priv_t* priv = calloc(1, sizeof(light_sensor_priv_t));
    priv->adc_chanel = adc_channel;
    priv->adc_handle = adc_handle;
    priv->interval_ms= interval;

    device_set_priv(dev,priv);
    dev->ops = &light_sensor_fops;
    ESP_LOGI(TAG, "probed: GPIO%d, chan=%d, atten=%d", adc_pin, adc_channel, adc_atten);
    return 0 ;
}

static int light_remove(device_t*dev)
{
    light_sensor_priv_t*priv = device_get_priv(dev);
    if(priv)
    {
        adc_oneshot_del_unit(priv->adc_handle);
        free(priv);
        device_set_priv(dev,NULL);
    }
    return 0 ;
}

//注册驱动

DRIVER_REGISTER(light_sensor, "gl5528,photoresistor", light_probe, light_remove);

static int light_sensor_read(device_t*dev,int *value)
{
    light_sensor_priv_t* priv = device_get_priv(dev);
    if(!priv||!value) return -1;
    int raw =0 ;
    adc_oneshot_read(priv->adc_handle,priv->adc_chanel,&raw);

    //把0~4095映射到0~100
    *value = (raw*100)/4095;
    return 0;
}

/* ── ioctl ── */
static int8_t light_sensor_ioctl(device_t* dev, int cmd, void* arg)
{
    switch (cmd) {
    case LIGHT_SENSOR_CMD_READ:
        if (!arg) return -1;
        return light_sensor_read(dev, (int*)arg);
    default:
        return -1;
    }
}
