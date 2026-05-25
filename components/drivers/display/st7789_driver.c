#include "st7789_driver.h"

#include "driver.h"
#include "hal_gpio.h"
#include "hal_spi_bus.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char* kTag = "st7789";

#define CMD_SWRESET    0x01
#define CMD_SLPOUT     0x11
#define CMD_INVON      0x21
#define CMD_DISPON     0x29
#define CMD_CASET      0x2A
#define CMD_RASET      0x2B
#define CMD_RAMWR      0x2C
#define CMD_MADCTL     0x36
#define CMD_COLMOD     0x3A
#define CMD_NORON      0x13

/* ── 驱动私有数据 ── */
typedef struct 
{
    hal_spi_bus_t    spi;         /* SPI bus 实例 */
    hal_gpio_config_t   gpio_dc;
    hal_gpio_config_t   gpio_rst;
    int             bl_pin;
    int             bl_active_high;
    int             width;
    int             height;
} st7789_priv_t;

/* ======================================================================== */
/*  底层 SPI 命令/数据写                                                     */
/* ======================================================================== */
static void write_cmd(st7789_priv_t* priv, uint8_t cmd)
{
    hal_gpio_set_level(priv->gpio_dc.pin, 0);   /* DC=0 → command */
    priv->spi.write(&priv->spi, &cmd, 1);
}

static void write_data(st7789_priv_t* priv, const uint8_t* data, int len)
{
    hal_gpio_set_level(priv->gpio_dc.pin, 1);   /* DC=1 → data */
    priv->spi.write(&priv->spi, data, len);
}

static void write_data_byte(st7789_priv_t* priv, uint8_t data)
{
    write_data(priv, &data, 1);
}

/* ======================================================================== */
/*  初始化序列                                                               */
/* ======================================================================== */
static void send_init_seq(st7789_priv_t* priv)
{
    write_cmd(priv, CMD_SWRESET);
    hal_gpio_set_level(priv->gpio_dc.pin, 1);   /* 恢复 DC */

    /* Power control A: 4-step VGMP timing, VGMP/VGSP */
    write_cmd(priv, 0xCB);
    write_data(priv, (uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);

    /* Power control B: gate/clock timing */
    write_cmd(priv, 0xCF);
    write_data(priv, (uint8_t[]){0x00, 0xC1, 0x30}, 3);

    /* DTCA: driver timing control A */
    write_cmd(priv, 0xE8);
    write_data(priv, (uint8_t[]){0x85, 0x00, 0x78}, 3);

    /* DTCB: driver timing control B */
    write_cmd(priv, 0xEA);
    write_data(priv, (uint8_t[]){0x00, 0x00}, 2);

    /* Power on sequence control */
    write_cmd(priv, 0xED);
    write_data(priv, (uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);

    /* Pump ratio control */
    write_cmd(priv, 0xF7);
    write_data_byte(priv, 0x20);

    /* Power control 1: GVDD = 4.75V, AVDD/VCL/VGH */
    write_cmd(priv, 0xC0);
    write_data_byte(priv, 0x23);   /* default */

    /* Power control 2: boost settings */
    write_cmd(priv, 0xC1);
    write_data_byte(priv, 0x10);

    /* VCOM control 1 */
    write_cmd(priv, 0xC5);
    write_data(priv, (uint8_t[]){0x3E, 0x28}, 2);

    /* VCOM control 2 */
    write_cmd(priv, 0xC7);
    write_data_byte(priv, 0x86);

    /* COLMOD: 16-bit color (0x55) */
    write_cmd(priv, CMD_COLMOD);
    write_data_byte(priv, 0x55);

    /* Frame Rate Control (60Hz) */
    write_cmd(priv, 0xB1);
    write_data(priv, (uint8_t[]){0x00, 0x13}, 2);

    /* Display Inversion Control */
    write_cmd(priv, 0xB4);
    write_data_byte(priv, 0x00);

    /* Display Function Control */
    write_cmd(priv, 0xB6);
    write_data(priv, (uint8_t[]){0x08, 0x82, 0x27}, 3);

    write_cmd(priv, CMD_INVON);          /* INVON (display inversion on) */

    /* Power Control 3: normal mode */
    write_cmd(priv, 0xC2);
    write_data(priv, (uint8_t[]){0xA7}, 1);

    write_cmd(priv, CMD_SLPOUT);         /* Sleep out */
}

/* ======================================================================== */
/*  Probe / Remove                                                          */
/* ======================================================================== */
static int st7789_probe(device_t* dev)
{
    /* 读取属性 */
    int dc_pin = -1, rst_pin = -1, bl_pin = -1;
    int width = 240, height = 240, bl_active = 1;
    int mosi = -1, sck = -1, freq = 80 * 1000 * 1000;

    device_get_prop_int(dev, "dc", &dc_pin);
    device_get_prop_int(dev, "reset", &rst_pin);
    device_get_prop_int(dev, "backlight", &bl_pin);
    device_get_prop_int(dev, "width", &width);
    device_get_prop_int(dev, "height", &height);
    device_get_prop_int(dev, "bl_active_high", &bl_active);
    device_get_prop_int(dev, "spi_freq_hz", &freq);

    /* 从 parent SPI device 读引脚 */
    device_t* parent = device_get_parent(dev);
    if (parent) {
        device_get_prop_int(parent, "mosi", &mosi);
        device_get_prop_int(parent, "sclk", &sck);
    }
    if (mosi < 0 || sck < 0) {
        ESP_LOGE(kTag, "missing SPI pin config");
        return -1;
    }

    st7789_priv_t* priv = (st7789_priv_t*)calloc(1, sizeof(st7789_priv_t));
    if (!priv) return -1;

    priv->width = width;
    priv->height = height;
    priv->bl_pin = bl_pin;
    priv->bl_active_high = bl_active;

    /* ── 初始化 DC/RST GPIO ── */
    priv->gpio_dc.pin = dc_pin;
    priv->gpio_dc.mode = HAL_GPIO_MODE_OUTPUT;
    hal_gpio_init(&priv->gpio_dc);

    if (rst_pin >= 0) {
        priv->gpio_rst.pin = rst_pin;
        priv->gpio_rst.mode = HAL_GPIO_MODE_OUTPUT;
        hal_gpio_init(&priv->gpio_rst);
    }

    if (bl_pin >= 0) {
        hal_gpio_config_t bl_cfg = { .pin = bl_pin, .mode = HAL_GPIO_MODE_OUTPUT };
        hal_gpio_init(&bl_cfg);
    }

    /* ── 初始化 SPI bus ── */
    hal_spi_bus_config_t bus_cfg = {
        .mosi = mosi, .miso = -1, .sclk = sck,
        .max_transfer_sz = width * height * 2 + 1024,
        .dma_chan = -1,
    };
    hal_spi_device_config_t dev_cfg = {
        .mode = 0, .clock_speed_hz = freq,
        .cs_pin = -1,        /* 7-wire SPI, no CS */
        .queue_size = 7,
    };

    hal_spi_bus_init_struct(&priv->spi);
    int ret = priv->spi.init(&priv->spi, &bus_cfg, &dev_cfg);
    if (ret != 0) {
        free(priv);
        return ret;
    }

    /* ── 硬件复位 ── */
    if (rst_pin >= 0) {
        hal_gpio_set_level(rst_pin, 0);
        /* short delay handled by caller */
        hal_gpio_set_level(rst_pin, 1);
    }

    /* ── 发送初始化序列 ── */
    send_init_seq(priv);

    write_cmd(priv, CMD_NORON);   /* Normal display on */
    write_cmd(priv, CMD_DISPON);  /* Display on */

    /* ── 点亮背光 ── */
    if (bl_pin >= 0) {
        hal_gpio_set_level(bl_pin, bl_active ? 1 : 0);
    }

    device_set_priv(dev, priv);
    ESP_LOGI(kTag, "probed: %dx%d ST7789 on SPI(MOSI=%d,SCK=%d,DC=%d,RST=%d)",
             width, height, mosi, sck, dc_pin, rst_pin);
    return 0;
}

static int st7789_remove(device_t* dev)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (priv) {
        priv->spi.deinit(&priv->spi);
        free(priv);
        device_set_priv(dev, NULL);
    }
    return 0;
}

DRIVER_REGISTER(st7789, "sitronix,st7789", st7789_probe, st7789_remove);

/* ======================================================================== */
/*  公开 API                                                                */
/* ======================================================================== */
int st7789_init(device_t* dev)
{
    /* probe 时已 init, 此函数 reserved */
    return 0;
}

int st7789_get_info(device_t* dev, st7789_info_t* info)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    info->width = priv->width;
    info->height = priv->height;
    return 0;
}

/* ── 设置裁剪窗口 ── */
static void set_window(st7789_priv_t* priv, int x, int y, int w, int h)
{
    write_cmd(priv, CMD_CASET);
    uint8_t data[] = { (uint8_t)(x >> 8), (uint8_t)(x & 0xFF),
                       (uint8_t)((x + w - 1) >> 8), (uint8_t)((x + w - 1) & 0xFF) };
    write_data(priv, data, 4);

    write_cmd(priv, CMD_RASET);
    uint8_t data2[] = { (uint8_t)(y >> 8), (uint8_t)(y & 0xFF),
                        (uint8_t)((y + h - 1) >> 8), (uint8_t)((y + h - 1) & 0xFF) };
    write_data(priv, data2, 4);
}

int st7789_fill_rect(device_t* dev, int x, int y, int w, int h, uint16_t color)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return -1;

    set_window(priv, x, y, w, h);
    write_cmd(priv, CMD_RAMWR);

    /* 填色: 16-bit RGB565, 重复 (w*h) 次 */
    hal_gpio_set_level(priv->gpio_dc.pin, 1);
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    int total = w * h;
    for (int i = 0; i < total; i++) {
        priv->spi.write(&priv->spi, &hi, 1);
        priv->spi.write(&priv->spi, &lo, 1);
    }
    return 0;
}

int st7789_fill_screen(device_t* dev, uint16_t color)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return -1;
    return st7789_fill_rect(dev, 0, 0, priv->width, priv->height, color);
}

int st7789_draw_bitmap(device_t* dev, int x, int y, int w, int h, const uint8_t* data)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv || !data) return -1;

    set_window(priv, x, y, w, h);
    write_cmd(priv, CMD_RAMWR);
    hal_gpio_set_level(priv->gpio_dc.pin, 1);
    priv->spi.write(&priv->spi, data, w * h * 2);
    return 0;
}

int st7789_set_backlight(device_t* dev, uint8_t brightness)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv || priv->bl_pin < 0) return -1;

    hal_gpio_set_level(priv->bl_pin, (brightness > 0) == priv->bl_active_high ? 1 : 0);
    return 0;
}

int st7789_write_ram(device_t* dev, int x, int y, int w, int h, const uint16_t* pixels)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv || !pixels) return -1;

    set_window(priv, x, y, w, h);
    write_cmd(priv, CMD_RAMWR);
    hal_gpio_set_level(priv->gpio_dc.pin, 1);
    priv->spi.write(&priv->spi, (const uint8_t*)pixels, w * h * 2);
    return 0;
}
