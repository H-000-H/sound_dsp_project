#include "hal_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
#include "esp_cpu.h"
#endif

void hal_cpu_emergency_stop_all_cores(void)
{
    portDISABLE_INTERRUPTS();

#if CONFIG_FREERTOS_NUMBER_OF_CORES > 1
    {
        int core = xPortGetCoreID();
        esp_cpu_stall((uint32_t)(core == 0 ? 1 : 0));
    }
#endif
}