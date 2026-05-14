#ifndef ST7789_H
#define ST7789_H
#include "config.hpp"
#if CONFIG_ENABLE_BSP_LCD_ST7789
#ifdef __cplusplus
extern "C"
{
#endif

#include "bsp_spi.h"
#include "driver/gpio.h"
#include <stddef.h>

/* ST7789 指令集 */
#define ST7789_SWRESET     0x01
#define ST7789_SLPOUT      0x11
#define ST7789_NORON       0x13
#define ST7789_INVOFF      0x20
#define ST7789_INVON       0x21
#define ST7789_DISPOFF     0x28
#define ST7789_DISPON      0x29
#define ST7789_CASET       0x2A
#define ST7789_RASET       0x2B
#define ST7789_RAMWR       0x2C
#define ST7789_COLMOD      0x3A
#define ST7789_MADCTL      0x36

#define LCD_SPI_CLK ((gpio_num_t)CONFIG_BSP_LCD_ST7789_PIN_CLK)
#define LCD_SPI_MOSI ((gpio_num_t)CONFIG_BSP_LCD_ST7789_PIN_MOSI)
#define LCD_SPI_DC ((gpio_num_t)CONFIG_BSP_LCD_ST7789_PIN_DC)
#define LCD_SPI_RST ((gpio_num_t)CONFIG_BSP_LCD_ST7789_PIN_RST)

/* 背光目前仍由独立 GPIO/PWM 控制。 */
#define LCD_SPI_BLCK ((gpio_num_t)CONFIG_BSP_LCD_ST7789_PIN_BLK)

/* 当前模组为 1.3 寸 240x240 的 7 引脚 ST7789，无独立 CS。 */
#define __LCD_CS_LOW() ((void)0)
#define __LCD_CS_HIGH() ((void)0)

#define __LCD_DC_HIGH() gpio_set_level(LCD_SPI_DC, 1)
#define __LCD_DC_LOW() gpio_set_level(LCD_SPI_DC, 0)
/* 背光为高电平点亮。 */
#if CONFIG_BSP_LCD_ST7789_BACKLIGHT_ACTIVE_HIGH
    #define __LCD_PWM_LOW() gpio_set_level(LCD_SPI_BLCK, 0)
    #define __LCD_PWM_HIGH() gpio_set_level(LCD_SPI_BLCK, 1)
#else
    #define __LCD_PWM_LOW() gpio_set_level(LCD_SPI_BLCK, 1)
    #define __LCD_PWM_HIGH() gpio_set_level(LCD_SPI_BLCK, 0)
#endif

/* 复位脚控制 */
#define __LCD_RESET_LOWER() gpio_set_level(LCD_SPI_RST, 0)
#define __LCD_RESET_HIGH() gpio_set_level(LCD_SPI_RST, 1)

typedef struct 
{
    uint8_t LCD_SHOW;
    uint8_t LCD_COL_MOD;
    uint16_t LCD_INIT_COLOR;
    uint16_t LCD_WIDTH;
    uint16_t LCD_HEIGHT;
    uint16_t LCD_X_OFFSET;
    uint16_t LCD_Y_OFFSET;
} bsp_lcd_handle_t;

/* arg 传入 LCD 面板配置。 */
void bsp_lcd_init(bsp_spi_handle* param, bsp_lcd_handle_t* arg);

void bsp_lcd_fill_rect(bsp_spi_handle* param,
                       uint16_t x_start,
                       uint16_t y_start,
                       uint16_t x_end,
                       uint16_t y_end,
                       uint16_t color,
                       bsp_lcd_handle_t* arg);

void bsp_lcd_draw_bitmap(bsp_spi_handle* param,
                         uint16_t x_start,
                         uint16_t y_start,
                         uint16_t x_end,
                         uint16_t y_end,
                         const uint16_t* color_data,
                         size_t pixel_count,
                         bsp_lcd_handle_t* arg);

void bsp_lcd_fill_screen(bsp_spi_handle* param, uint16_t color, bsp_lcd_handle_t* arg);

void bsp_lcd_clear(bsp_spi_handle* param, bsp_lcd_handle_t* arg);

#ifdef __cplusplus
}
#endif

#endif
#endif
