#include "hal_i2s_bus.h"
#include "hal_spi_bus.h"

#include "esp_private/periph_ctrl.h"
#include "soc/periph_defs.h"

void hal_i2s_force_stop(void)
{
#if SOC_I2S_NUM > 0
    periph_module_reset(PERIPH_I2S0_MODULE);
    periph_module_disable(PERIPH_I2S0_MODULE);
#endif
#if SOC_I2S_NUM > 1
    periph_module_reset(PERIPH_I2S1_MODULE);
    periph_module_disable(PERIPH_I2S1_MODULE);
#endif
}

void hal_spi_force_stop(void)
{
#if SOC_SPI_PERIPH_NUM > 0
    periph_module_reset(PERIPH_SPI2_MODULE);
    periph_module_disable(PERIPH_SPI2_MODULE);
#endif
#if SOC_SPI_PERIPH_NUM > 1
    periph_module_reset(PERIPH_SPI3_MODULE);
    periph_module_disable(PERIPH_SPI3_MODULE);
#endif
}