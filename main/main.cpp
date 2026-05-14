#include "app_freertos.hpp"
#include "main.hpp"

EXTERN_C void app_main(void)
{
    lv_init();
    app_freertos_init();
}
