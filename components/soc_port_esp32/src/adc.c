#include "hal_adc.h"

#include "device.h"
#include "driver.h"
#include "esp_adc/adc_oneshot.h"
#include "osal.h"
#include "VFS.h"
#include <string.h>

static const char* TAG = "hal_adc";

typedef struct {
    adc_oneshot_unit_handle_t handle;
    uint32_t configured_mask;
} adc_priv_t;

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define ADC_PRIV_POOL_SIZE 2
static adc_priv_t s_adc_pool[ADC_PRIV_POOL_SIZE];
static uint8_t s_adc_used[ADC_PRIV_POOL_SIZE];

static adc_atten_t adc_atten_from_int(int atten)
{
    switch (atten) {
    case 0: return ADC_ATTEN_DB_0;
    case 1: return ADC_ATTEN_DB_2_5;
    case 2: return ADC_ATTEN_DB_6;
    case 3: return ADC_ATTEN_DB_12;
    default: return ADC_ATTEN_DB_12;
    }
}

static adc_bitwidth_t adc_width_from_int(int bitwidth)
{
    return bitwidth == 9 ? ADC_BITWIDTH_9 :
           bitwidth == 10 ? ADC_BITWIDTH_10 :
           bitwidth == 11 ? ADC_BITWIDTH_11 :
           ADC_BITWIDTH_12;
}

static int adc_config_channel_once(adc_priv_t* priv, int channel, int atten, int bitwidth)
{
    if (!priv || channel < 0 || channel >= 32) return -1;
    uint32_t bit = 1UL << (uint32_t)channel;
    if (priv->configured_mask & bit) return 0;

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = adc_atten_from_int(atten),
        .bitwidth = adc_width_from_int(bitwidth),
    };
    esp_err_t ret = adc_oneshot_config_channel(priv->handle, channel, &chan_cfg);
    if (ret != ESP_OK) return (ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
    priv->configured_mask |= bit;
    return 0;
}

static int adc_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)
{
    (void)arg_len;
    adc_priv_t* priv = (adc_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    switch (cmd) {
    case ADC_CMD_READ_RAW: {
        if (!arg) return -1;
        adc_read_arg_t* a = (adc_read_arg_t*)arg;
        if (!a->out_raw) return -1;
        int ret = adc_config_channel_once(priv, a->channel, a->atten, a->bitwidth);
        if (ret != 0) return ret;
        esp_err_t err = adc_oneshot_read(priv->handle, a->channel, a->out_raw);
        return (err == ESP_OK) ? 0 : (err == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
    }
    case ADC_CMD_STOP:
        /* oneshot 模式无连续转换, 仅作为安全关闭信号 */
        return 0;

    default:
        return -1;
    }
}

static const file_operation_t adc_fops = {
    .ioctl = adc_ioctl,
};

static int adc_probe(device_t* dev)
{
    int unit = 1;
    device_get_prop_int(dev, "unit", &unit);
    device_get_prop_int(dev, "reg", &unit);

    int pool_idx = osal_pool_claim(s_adc_used, ADC_PRIV_POOL_SIZE);
    if (pool_idx < 0) return VFS_ERR_NOMEM;
    adc_priv_t* priv = &s_adc_pool[pool_idx];
    memset(priv, 0, sizeof(*priv));

    int ret = 0;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = unit == 2 ? ADC_UNIT_2 : ADC_UNIT_1,
    };
    esp_err_t esp_ret = adc_oneshot_new_unit(&unit_cfg, &priv->handle);
    if (esp_ret != ESP_OK) {
        DRV_LOGE(TAG, "adc_oneshot_new_unit failed: %d", esp_ret);
        ret = (esp_ret == ESP_ERR_NO_MEM) ? VFS_ERR_NOMEM : VFS_ERR_IO;
        goto err_pool;
    }

    device_set_priv(dev, priv);
    dev->ops = &adc_fops;
    DRV_LOGI(TAG, "ADC probed: unit=%d", unit);
    return 0;

err_pool:
    osal_pool_release(s_adc_used, ADC_PRIV_POOL_SIZE, pool_idx);
    return ret;
}

static int adc_remove(device_t* dev)
{
    adc_priv_t* priv = (adc_priv_t*)device_get_priv(dev);
    if (priv) {
        adc_oneshot_del_unit(priv->handle);
        for (int i = 0; i < ADC_PRIV_POOL_SIZE; i++) { if (&s_adc_pool[i] == priv) { osal_pool_release(s_adc_used, ADC_PRIV_POOL_SIZE, i); break; } }
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(adc, "esp32,adc", adc_probe, adc_remove);
