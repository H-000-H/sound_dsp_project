#include "bsp_spi.h"
#include "st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#if CONFIG_ENABLE_BSP_LCD_ST7789

static SemaphoreHandle_t lcd_mutex = NULL;

/* 预分配的 DMA 缓冲：避免每次刷图时 malloc/free 造成堆碎片 */
#define LCD_DMA_BUF_PIXELS 2048
#define LCD_DMA_BUF_SIZE   (LCD_DMA_BUF_PIXELS * 2)

static uint8_t* lcd_dma_buf = NULL;

static void lcd_lock(void)
{
    if (lcd_mutex != NULL)
    {
        xSemaphoreTake(lcd_mutex, portMAX_DELAY);
    }
}

static void lcd_unlock(void)
{
    if (lcd_mutex != NULL)
    {
        xSemaphoreGive(lcd_mutex);
    }
}

static void lcd_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_SPI_DC) | (1ULL << LCD_SPI_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* 7 引脚模组没有独立 CS，上电后先把 DC 拉高。 */
    __LCD_DC_HIGH();

    ESP_LOGI("ST7789_7WIRE", "GPIO init: CLK=%d MOSI=%d DC=%d RST=%d (BLK by PWM, no CS)",
             LCD_SPI_CLK, LCD_SPI_MOSI, LCD_SPI_DC, LCD_SPI_RST);
}

/* 发送命令：7 引脚模组通过 DC 区分命令与数据。 */
static void lcd_send_cmd(bsp_spi_handle* param, uint8_t cmd)
{
    __LCD_DC_LOW();
    bsp_spi_send(param,&cmd,1);
}

/* 发送单字节数据。 */
static void lcd_send_data(bsp_spi_handle* param, uint8_t data)
{
    __LCD_DC_HIGH();
    bsp_spi_send(param,&data,1);
}

/* 先发命令，再连续发送参数数据。 */
static void lcd_send_cmd_data(bsp_spi_handle* param, uint8_t cmd, const uint8_t* data, uint8_t len)
{
    __LCD_DC_LOW();
    bsp_spi_send(param, &cmd, 1);
    __LCD_DC_HIGH();
    if (len > 0 && data != NULL)
    {
        bsp_spi_send(param, data, len);
    }
}

/* 批量发送像素数据，长度使用 32 位以适配 DMA 大块传输。 */
static void lcd_send_data_buffer(bsp_spi_handle* param,const uint8_t* buf,uint32_t len)
{
    __LCD_DC_HIGH();
    bsp_spi_send(param, buf, len);
}

/* 设置显示窗口。 */
static void lcd_set_window_impl(bsp_spi_handle* param,uint16_t x_start,uint16_t x_end,uint16_t y_start,uint16_t y_end)
{
    uint8_t data[4];
    data[0]=(uint8_t)((x_start>>8) & 0xff);
    data[1]=(uint8_t)(x_start & 0xff);
    data[2]=(uint8_t)((x_end>>8) & 0xff);
    data[3]=(uint8_t)(x_end&  0xff);
    lcd_send_cmd_data(param,ST7789_CASET,data,4);

    data[0]=(uint8_t)((y_start>>8)&0xff);
    data[1]=(uint8_t)(y_start&0xff);
    data[2]=(uint8_t)((y_end>>8)&0xff);
    data[3]=(uint8_t)(y_end&0xff);
    lcd_send_cmd_data(param,ST7789_RASET,data,4);
}

static int lcd_prepare_region(const bsp_lcd_handle_t* handle,
                              uint16_t x_start,
                              uint16_t y_start,
                              uint16_t x_end,
                              uint16_t y_end,
                              uint16_t* phy_x_start,
                              uint16_t* phy_y_start,
                              uint16_t* phy_x_end,
                              uint16_t* phy_y_end,
                              uint32_t* pixel_count)
{
    if (handle == NULL || x_end < x_start || y_end < y_start)
    {
        return 0;
    }

    if (x_end >= handle->LCD_WIDTH || y_end >= handle->LCD_HEIGHT)
    {
        ESP_LOGE("ST7789_7WIRE", "invalid region: x=%u-%u y=%u-%u, panel=%ux%u",
                 x_start, x_end, y_start, y_end, handle->LCD_WIDTH, handle->LCD_HEIGHT);
        return 0;
    }

    *phy_x_start = handle->LCD_X_OFFSET + x_start;
    *phy_x_end = handle->LCD_X_OFFSET + x_end;
    *phy_y_start = handle->LCD_Y_OFFSET + y_start;
    *phy_y_end = handle->LCD_Y_OFFSET + y_end;
    *pixel_count = (uint32_t)(x_end - x_start + 1) * (uint32_t)(y_end - y_start + 1);
    return 1;
}

static void lcd_begin_write_region(bsp_spi_handle* param,
                                   uint16_t x_start,
                                   uint16_t y_start,
                                   uint16_t x_end,
                                   uint16_t y_end)
{
    lcd_set_window_impl(param, x_start, x_end, y_start, y_end);
    __LCD_DC_LOW();
    uint8_t ramwr_cmd = ST7789_RAMWR;
    bsp_spi_send(param, &ramwr_cmd, 1);
    __LCD_DC_HIGH();
}

static void lcd_fill_rect_impl(bsp_spi_handle* param,
                               uint16_t x_start,
                               uint16_t y_start,
                               uint16_t x_end,
                               uint16_t y_end,
                               uint16_t color,
                               bsp_lcd_handle_t* arg)
{
    bsp_lcd_handle_t* handle = arg;
    uint16_t phy_x_start = 0;
    uint16_t phy_y_start = 0;
    uint16_t phy_x_end = 0;
    uint16_t phy_y_end = 0;
    uint32_t total_pixels = 0;

    if (!lcd_prepare_region(handle, x_start, y_start, x_end, y_end,
                            &phy_x_start, &phy_y_start, &phy_x_end, &phy_y_end, &total_pixels))
    {
        return;
    }

    const uint32_t region_width = (uint32_t)(x_end - x_start + 1);
    const uint32_t target_pixels_per_block = LCD_DMA_BUF_PIXELS;
    const uint32_t rows_per_block = (target_pixels_per_block / region_width) > 0
                                      ? (target_pixels_per_block / region_width)
                                      : 1;
    const uint32_t pixels_per_block = region_width * rows_per_block;
    const uint32_t bytes_per_block = pixels_per_block * 2;
    const uint8_t color_high = (uint8_t)((color >> 8) & 0xff);
    const uint8_t color_low = (uint8_t)(color & 0xff);

    /* 使用预分配的 DMA 缓冲（在 bsp_lcd_init 中分配）*/
    if (lcd_dma_buf == NULL || bytes_per_block > LCD_DMA_BUF_SIZE)
    {
        ESP_LOGE("ST7789_7WIRE", "DMA buffer unavailable for fill_rect");
        return;
    }

    for (uint32_t i = 0; i < pixels_per_block; ++i)
    {
        lcd_dma_buf[i * 2] = color_high;
        lcd_dma_buf[i * 2 + 1] = color_low;
    }

    lcd_begin_write_region(param, phy_x_start, phy_y_start, phy_x_end, phy_y_end);

    uint32_t sent_pixels = 0;
    while (sent_pixels < total_pixels)
    {
        const uint32_t remain = total_pixels - sent_pixels;
        const uint32_t pixels_to_send = remain > pixels_per_block ? pixels_per_block : remain;
        lcd_send_data_buffer(param, lcd_dma_buf, pixels_to_send * 2);
        sent_pixels += pixels_to_send;
    }
}

static void lcd_draw_bitmap_impl(bsp_spi_handle* param,
                                 uint16_t x_start,
                                 uint16_t y_start,
                                 uint16_t x_end,
                                 uint16_t y_end,
                                 const uint16_t* color_data,
                                 size_t pixel_count,
                                 bsp_lcd_handle_t* arg)
{
    bsp_lcd_handle_t* handle = arg;
    uint16_t phy_x_start = 0;
    uint16_t phy_y_start = 0;
    uint16_t phy_x_end = 0;
    uint16_t phy_y_end = 0;
    uint32_t total_pixels = 0;

    if (color_data == NULL)
    {
        ESP_LOGE("ST7789_7WIRE", "draw_bitmap color_data is null");
        return;
    }

    if (!lcd_prepare_region(handle, x_start, y_start, x_end, y_end,
                            &phy_x_start, &phy_y_start, &phy_x_end, &phy_y_end, &total_pixels))
    {
        return;
    }

    if (pixel_count < total_pixels)
    {
        ESP_LOGE("ST7789_7WIRE", "draw_bitmap pixel_count too small: got=%u need=%u",
                 (unsigned)pixel_count, (unsigned)total_pixels);
        return;
    }

    const uint32_t pixels_per_block = LCD_DMA_BUF_PIXELS;

    /* 使用预分配的 DMA 缓冲（在 bsp_lcd_init 中分配）*/
    if (lcd_dma_buf == NULL)
    {
        ESP_LOGE("ST7789_7WIRE", "DMA buffer unavailable for draw_bitmap");
        return;
    }

    lcd_begin_write_region(param, phy_x_start, phy_y_start, phy_x_end, phy_y_end);

    uint32_t sent_pixels = 0;
    while (sent_pixels < total_pixels)
    {
        const uint32_t remain = total_pixels - sent_pixels;
        const uint32_t pixels_to_send = remain > pixels_per_block ? pixels_per_block : remain;

        for (uint32_t i = 0; i < pixels_to_send; ++i)
        {
            const uint16_t color = color_data[sent_pixels + i];
            lcd_dma_buf[i * 2] = (uint8_t)((color >> 8) & 0xff);
            lcd_dma_buf[i * 2 + 1] = (uint8_t)(color & 0xff);
        }

        lcd_send_data_buffer(param, lcd_dma_buf, pixels_to_send * 2);
        sent_pixels += pixels_to_send;
    }
}

void bsp_lcd_init(bsp_spi_handle* param, bsp_lcd_handle_t* arg)
{
    /* 预分配 DMA 缓冲（仅在首次调用时分配）*/
    if (lcd_dma_buf == NULL)
    {
        lcd_dma_buf = (uint8_t*)heap_caps_malloc(LCD_DMA_BUF_SIZE, MALLOC_CAP_DMA);
        if (lcd_dma_buf == NULL)
        {
            ESP_LOGE("ST7789", "failed to allocate DMA buffer, fallback to normal heap");
            lcd_dma_buf = (uint8_t*)malloc(LCD_DMA_BUF_SIZE);
        }
    }

    ESP_LOGI("ST7789_7WIRE", "init start");
    if (lcd_mutex == NULL)
    {
        lcd_mutex = xSemaphoreCreateMutex();
    }
    lcd_gpio_init();
    bsp_lcd_handle_t* handle = arg;

    /* 硬件复位。 */
    __LCD_RESET_LOWER();
    vTaskDelay(pdMS_TO_TICKS(5));
    __LCD_RESET_HIGH();
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 软件复位。 */
    lcd_send_cmd(param, ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 退出睡眠。 */
    lcd_send_cmd(param,ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* MADCTL: 0x00 对应 RGB 排列，解决红蓝反转问题。 */
    lcd_send_cmd(param,ST7789_MADCTL);
    lcd_send_data(param, 0x00);

    /* 颜色格式设为 16 位 RGB565。 */
    lcd_send_cmd(param,ST7789_COLMOD);
    lcd_send_data(param, 0x55);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 设置列地址范围为 0~239。 */
    lcd_send_cmd(param,ST7789_CASET);
    uint8_t caset_dat[] = {0x00, 0x00, 0x00, 0xEF};
    lcd_send_data_buffer(param, caset_dat, 4);

    /* 设置行地址范围为 0~239。 */
    lcd_send_cmd(param,ST7789_RASET);
    uint8_t raset_dat[] = {0x00, 0x00, 0x00, 0xEF};
    lcd_send_data_buffer(param, raset_dat, 4);

    /* IPS 屏常见需要开启反色显示。 */
    lcd_send_cmd(param, ST7789_INVON);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 进入正常显示。 */
    lcd_send_cmd(param,ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 开启显示输出。 */
    lcd_send_cmd(param,ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(20));
}

void bsp_lcd_fill_rect(bsp_spi_handle* param,
                       uint16_t x_start,
                       uint16_t y_start,
                       uint16_t x_end,
                       uint16_t y_end,
                       uint16_t color,
                       bsp_lcd_handle_t* arg)
{
    lcd_lock();
    lcd_fill_rect_impl(param, x_start, y_start, x_end, y_end, color, arg);
    lcd_unlock();
}

void bsp_lcd_draw_bitmap(bsp_spi_handle* param,
                         uint16_t x_start,
                         uint16_t y_start,
                         uint16_t x_end,
                         uint16_t y_end,
                         const uint16_t* color_data,
                         size_t pixel_count,
                         bsp_lcd_handle_t* arg)
{
    lcd_lock();
    lcd_draw_bitmap_impl(param, x_start, y_start, x_end, y_end, color_data, pixel_count, arg);
    lcd_unlock();
}

void bsp_lcd_fill_screen(bsp_spi_handle* param, uint16_t color, bsp_lcd_handle_t* arg)
{
    bsp_lcd_handle_t* handle = arg;
    lcd_lock();
    lcd_fill_rect_impl(param,
                       0,
                       0,
                       handle->LCD_WIDTH - 1,
                       handle->LCD_HEIGHT - 1,
                       color,
                       handle);
    lcd_unlock();
}
void bsp_lcd_clear(bsp_spi_handle* param, bsp_lcd_handle_t* arg)
{
    bsp_lcd_handle_t* handle = arg;
    lcd_lock();
    lcd_fill_rect_impl(param,
                       0,
                       0,
                       handle->LCD_WIDTH - 1,
                       handle->LCD_HEIGHT - 1,
                       handle->LCD_INIT_COLOR,
                       handle);
    lcd_unlock();
}
#endif


