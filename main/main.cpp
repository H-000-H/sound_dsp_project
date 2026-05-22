#include "main.hpp"
#include "system_runtime.hpp"
#include "ui_service.hpp"

void lvgl_main(void);

EXTERN_C void app_main(void)
{
    UiService::set_entry(lvgl_main);
    SystemRuntime::getInstance().start();
}
