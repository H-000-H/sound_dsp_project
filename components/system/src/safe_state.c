#include "safe_state.h"

#include "board_config.h"
#include "hal_cpu.h"
#include "hal_i2s_bus.h"
#include "hal_pwm.h"
#include "hal_spi_bus.h"
#include "system_log.hpp"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/rtc.h"

/*
 * Bootloop 退避计数器 (RTC 慢速内存, 掉电不丢失).
 * 连续 Panic/软件复位 ≥ 5 次 → 进入永久安全锁死,
 * 拒绝一切 Flash 写入和软重启, 防止 100,000 次擦写烧穿 SPI Flash.
 */
RTC_DATA_ATTR static uint32_t s_panic_counter = 0;

bool safe_state_check_bootloop(void)
{
    if (s_panic_counter >= 5)
    {
        enter_safe_state("BOOTLOOP DETECTED > 5 — SYSTEM FROZEN");
        return false;
    }
    s_panic_counter++;
    return true;
}

void safe_state_clear_bootloop(void)
{
    s_panic_counter = 0;
}

/*
 * Safe State 蜂鸣器: LEDC 硬件外设
 * ─────────────────────────────
 * 2Hz, 50% duty. 一旦配置完成, LEDC 外设完全自主运行,
 * 不依赖 FreeRTOS 调度器, 不受 vTaskSuspendAll() 影响.
 *
 * 时序:
 *   500ms HIGH ─┐  500ms LOW
 *               ├──────────────
 *               └──    ──┘
 *
 * 硬件资源: LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0
 * 在 enter_safe_state() 中被独占, 与正常 LEDC 背光通道冲突
 * 但安全状态下背光已不重要.
 */
static void safe_state_config_buzzer(void)
{
    ledc_timer_config_t tmr = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 2,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tmr);

    ledc_channel_config_t ch = {
        .gpio_num   = BOARD_SAFE_STATE_BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 4096,   /* 50% @ 13-bit (8191 max) */
        .hpoint     = 0,
        .flags      = {.output_invert = 0},
    };
    ledc_channel_config(&ch);
}

void enter_safe_state(const char* reason)
{
    const char* what = reason ? reason : "unknown";

    hal_pwm_force_stop_all();
    hal_i2s_force_stop();
    hal_spi_force_stop();

    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << BOARD_SAFE_STATE_FAULT_LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_cfg);
    gpio_set_level(BOARD_SAFE_STATE_FAULT_LED_PIN, 1);

    safe_state_config_buzzer();

    vTaskSuspendAll();
    portDISABLE_INTERRUPTS();

    while (1)
    {
        for (volatile int i = 0; i < 500000; i++) { __asm__ volatile("nop"); }
        gpio_set_level(BOARD_SAFE_STATE_FAULT_LED_PIN, 0);
        for (volatile int i = 0; i < 500000; i++) { __asm__ volatile("nop"); }
        gpio_set_level(BOARD_SAFE_STATE_FAULT_LED_PIN, 1);
    }
}

void IRAM_ATTR safe_state_nmi_emergency_stamp(void)
{
    WRITE_PERI_REG(RTC_CNTL_STORE0_REG, 0xDEADBEEF);

    gpio_set_level(BOARD_SAFE_STATE_FAULT_LED_PIN, 1);

    /* LEDC 已经配置完毕, NMI 中不需要重复配置.
     * 蜂鸣器由硬件 LEDC 外设自主维持. */
}