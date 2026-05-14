#include "bsp_blue.h"

#if CONFIG_ENABLE_BSP_BLUE
#if defined(SOC_BT_SUPPORTED) && SOC_BT_SUPPORTED && defined(SOC_BT_CLASSIC_SUPPORTED) && SOC_BT_CLASSIC_SUPPORTED
void bsp_blue_init(bsp_blue_handle_t* handle)
{
    if(handle==NULL) return;
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(handle->MODE[0]));//释放内存

    ESP_ERROR_CHECK(esp_bt_controller_init(&handle->bt_cfg));

    ESP_ERROR_CHECK(esp_bt_controller_enable(handle->MODE[1]));

    ESP_ERROR_CHECK(esp_bluedroid_init());// 初始化并分配蓝牙资源，必须先于每个蓝牙组件。

    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_dev_set_device_name(handle->bluebooth_name));

    ESP_ERROR_CHECK(esp_a2d_register_callback(handle->a2d_cb));

    ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(handle->a2d_source_data_cb));

    ESP_ERROR_CHECK(esp_avrc_ct_init());//初始化AVRC控制器在source之前调用

    ESP_ERROR_CHECK(esp_a2d_source_init());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(handle->gap_cb));

    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(handle->c_mode,handle->d_mode));

    const uint8_t inq_len = handle->inq_len != 0 ? handle->inq_len : CONFIG_BSP_BLUE_DEFAULT_INQ_LEN;
    const uint8_t num_rsps = handle->num_rsps != 0 ? handle->num_rsps : CONFIG_BSP_BLUE_DEFAULT_NUM_RSPS;
    ESP_ERROR_CHECK(esp_bt_gap_start_discovery(handle->i_mode, inq_len, num_rsps));//扫描时间1.28*inq_len 0表示无响应限制
}
#endif
#endif
