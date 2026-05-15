#include "app_freertos.hpp"
#include "esp_heap_caps.h"

namespace mqtt_tasks
{
    void mqtt_task(void* param)
    {
        ThingsCloudApp& app = ThingsCloudApp::get_instance();
        app.set_rgb_led(CONFIG_BSP_RGB_LED_GPIO);
        app.init();

        /* 启动时默认 WiFi 开，自动连接并启动 MQTT */
        app.start();

        uint32_t cnt = 0;
        app.run([&cnt](float* t, uint32_t* h)
        {
            *t = 25.0f + (cnt % 10);
            *h = 50 + (cnt % 20);
            cnt++;
        });
    }
}

namespace lcd_tasks
{
    void lcd_task(void*param)
    {
        lvgl_main();
    }
}

/*蓝牙任务 s3无经典蓝牙
namespace Blue_Booth
{
    void blue_task(void*param)
    {
        auto* blue = factory_config::blue::get_blue();
        blue->init();
        uint8_t remote_bda[6] = {0x20, 0x3B, 0x34, 0x6B, 0x60, 0x9F};
        blue->connect(remote_bda);
        while(true)
        {
            ESP_LOGI("blue","");
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
*/
/*开启lvgl必须重写分区表并且修改sdkconfig
必须开启PRAM不然内存在此项目必然不够 将CONFIG_SPIRAM_SUPPORT设置为y 
将freertos的任务放static上，全局bss开启外部存储  双缓冲dma放在psram上(会慢但可以释放ram)目前采用还是dma里面但本人用ifndef可以去自行裁剪
*/
void app_freertos_init()
{
    // 将 LCD 任务栈分配在 PSRAM 中
    // Lottie JSON 解析涉及深度递归调用，需要更大的栈空间（实测 8KB 不够，至少 24KB）
    const uint32_t lcd_stack_size = 24 * 1024;
    StackType_t* lcd_task_stack = (StackType_t*)heap_caps_malloc(lcd_stack_size, MALLOC_CAP_SPIRAM);
    StaticTask_t* lcd_task_tcb = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    
    if (lcd_task_stack && lcd_task_tcb) 
    {
        xTaskCreateStaticPinnedToCore(lcd_tasks::lcd_task, "lcd_task", lcd_stack_size, nullptr, 5, lcd_task_stack, lcd_task_tcb, CORE_ONE);
    } 
    else 
    {
        if (lcd_task_stack) heap_caps_free(lcd_task_stack);
        if (lcd_task_tcb) heap_caps_free(lcd_task_tcb);
        // 回退方案
        xTaskCreatePinnedToCore(lcd_tasks::lcd_task, "lcd_task", lcd_stack_size, nullptr, 5, nullptr, CORE_ONE);
    }

    xTaskCreatePinnedToCore(mqtt_tasks::mqtt_task, "mqtt_task", 4*1024, nullptr, 5, nullptr, CORE_TWO);

}

