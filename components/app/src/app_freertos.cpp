#include "app_freertos.hpp"

void app_freertos_init()
{
    SystemRuntime::getInstance().start();
}
