#include "st7789_driver.h"

#include "driver.h"
#include "VFS.h"
#include "vfs_gpio.h"
#include "vfs_pwm.h"
#include "hal_gpio_fast.h"
#include "osal.h"
#include "display.h"

#include <string.h>
#include "board_config.h"

static const char* kTag = "st7789";

/* ── BSS 静态池（禁止运行时动态分配） ── */
#define ST7789_MAX_WIDTH     320
#define ST7789_LINE_BUF_SIZE (ST7789_MAX_WIDTH * 2)

/* ST7789_COUNT 由 board_config.h 提供, 在此之下声明 BSS 数组 */

#define CMD_SWRESET    0x01
#define CMD_SLPIN      0x10
#define CMD_SLPOUT     0x11
#define CMD_INVON      0x21
#define CMD_DISPOFF    0x28
#define CMD_DISPON     0x29
#define CMD_CASET      0x2A
#define CMD_RASET      0x2B
#define CMD_RAMWR      0x2C
#define CMD_COLMOD     0x3A
#define CMD_NORON      0x13

#define ST7789_DEFAULT_SPI_CHUNK 4096U

/* 扁平初始化序列: cmd, len, [data...] */
static const uint8_t kInitSeq[] = {
    CMD_SWRESET, 0,
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    0xCF, 3, 0x00, 0xC1, 0x30,
    0xE8, 3, 0x85, 0x00, 0x78,
    0xEA, 2, 0x00, 0x00,
    0xED, 4, 0x64, 0x03, 0x12, 0x81,
    0xF7, 1, 0x20,
    0xC0, 1, 0x23,
    0xC1, 1, 0x10,
    0xC5, 2, 0x3E, 0x28,
    0xC7, 1, 0x86,
    CMD_COLMOD, 1, 0x55,
    0xB1, 2, 0x00, 0x13,
    0xB4, 1, 0x00,
    0xB6, 3, 0x08, 0x82, 0x27,
    CMD_INVON, 0,
    0xC2, 1, 0xA7,
    CMD_SLPOUT, 0,
};

static const struct display_ops st7789_display_ops;

typedef struct
{
    display_if_priv_t   display_hdr;    /* 子系统魔术头, 通过 subsys_priv 显式绑定 */
    device_t*       spi_dev;
    device_t*       gpio_dev;
    hal_gpio_config_t   gpio_dc;
    hal_gpio_config_t   gpio_rst;
    device_t*       bl_pwm_dev;
    int             bl_pin;
    int             bl_active_high;
    int             width;
    int             height;
    size_t          spi_chunk;
    uint8_t*        line_buf;
    size_t          line_buf_len;
    uint8_t         bl_brightness;
    int             pool_idx;
    osal_mutex_t*   priv_lock;
    uint8_t         priv_lock_storage[OSAL_MUTEX_STORAGE_SIZE];
} st7789_priv_t;

static int st7789_init(device_t* dev);
static int st7789_open(device_t* dev, void* arg);
static int st7789_close(device_t* dev);
static int st7789_suspend(device_t* dev);
static int st7789_resume(device_t* dev);
static const file_operation_t st7789_fops = {
    .open    = st7789_open,
    .close   = st7789_close,
    .init    = st7789_init,
    .suspend = st7789_suspend,
    .resume  = st7789_resume,
};


static st7789_priv_t __attribute__((aligned(32))) s_st7789_pool[ST7789_COUNT];
static uint8_t s_st7789_used[ST7789_COUNT];
static uint8_t __attribute__((aligned(32))) s_st7789_line_buf[ST7789_COUNT][ST7789_LINE_BUF_SIZE];

static int spi_write_chunked(st7789_priv_t* priv, const uint8_t* data, size_t len, uint32_t timeout_ms)
{
    if (!priv || !priv->spi_dev || (!data && len > 0)) return VFS_ERR_INVAL;
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > priv->spi_chunk) chunk = priv->spi_chunk;
        int ret = device_write(priv->spi_dev, data + offset, chunk, timeout_ms);
        if (ret != 0) return ret;
        offset += chunk;
    }
    return 0;
}

static int write_cmd(st7789_priv_t* priv, uint8_t cmd, uint32_t timeout_ms)
{
    hal_gpio_set_level_fast(priv->gpio_dc.pin, 0);
    return spi_write_chunked(priv, &cmd, 1, timeout_ms);
}

static int write_data(st7789_priv_t* priv, const uint8_t* data, size_t len, uint32_t timeout_ms)
{
    hal_gpio_set_level_fast(priv->gpio_dc.pin, 1);
    return spi_write_chunked(priv, data, len, timeout_ms);
}

static int rect_in_bounds(const st7789_priv_t* priv, int x, int y, int w, int h)
{
    if (!priv || x < 0 || y < 0 || w <= 0 || h <= 0) return 0;
    if (x > priv->width - w || y > priv->height - h) return 0;
    return 1;
}

static int clip_rect(const st7789_priv_t* priv, int* x, int* y, int* w, int* h)
{
    if (!priv || !x || !y || !w || !h || *w <= 0 || *h <= 0) return VFS_ERR_INVAL;

    if (*x < 0) {
        *w += *x;
        *x = 0;
    }
    if (*y < 0) {
        *h += *y;
        *y = 0;
    }
    if (*x >= priv->width || *y >= priv->height || *w <= 0 || *h <= 0) return VFS_ERR_INVAL;
    if (*w > priv->width - *x) *w = priv->width - *x;
    if (*h > priv->height - *y) *h = priv->height - *y;
    return (*w > 0 && *h > 0) ? 0 : VFS_ERR_INVAL;
}

static int set_window(st7789_priv_t* priv, int x, int y, int w, int h, uint32_t timeout_ms)
{
    uint16_t x0 = (uint16_t)x;
    uint16_t y0 = (uint16_t)y;
    uint16_t x1 = (uint16_t)(x + w - 1);
    uint16_t y1 = (uint16_t)(y + h - 1);

    uint8_t col[] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
    };
    uint8_t row[] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
    };

    int ret = write_cmd(priv, CMD_CASET, timeout_ms);
    if (ret != 0) return ret;
    ret = write_data(priv, col, sizeof(col), timeout_ms);
    if (ret != 0) return ret;
    ret = write_cmd(priv, CMD_RASET, timeout_ms);
    if (ret != 0) return ret;
    return write_data(priv, row, sizeof(row), timeout_ms);
}

static int send_init_seq(st7789_priv_t* priv, uint32_t timeout_ms)
{
    size_t i = 0;
    while (i < sizeof(kInitSeq)) {
        uint8_t cmd = kInitSeq[i++];
        uint8_t len = kInitSeq[i++];
        int ret = write_cmd(priv, cmd, timeout_ms);
        if (ret != 0) return ret;
        if (len > 0) {
            ret = write_data(priv, &kInitSeq[i], len, timeout_ms);
            if (ret != 0) return ret;
            i += len;
        }
    }
    hal_gpio_set_level_fast(priv->gpio_dc.pin, 1);
    return 0;
}

static int st7789_open(device_t* dev, void* arg)
{
    (void)arg;
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    if (priv->display_hdr.magic != DISPLAY_IF_MAGIC) return VFS_ERR_INVAL;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;

    if (priv->gpio_rst.mode == HAL_GPIO_MODE_OUTPUT) {
        hal_gpio_set_level_fast(priv->gpio_rst.pin, 0);
        osal_delay_ms(120);
        hal_gpio_set_level_fast(priv->gpio_rst.pin, 1);
        osal_delay_ms(120);
    }

    int ret = send_init_seq(priv, ST7789_TIMEOUT_CMD_MS);
    if (ret == 0) osal_delay_ms(120);
    if (ret == 0) ret = write_cmd(priv, CMD_NORON, ST7789_TIMEOUT_CMD_MS);
    if (ret == 0) ret = write_cmd(priv, CMD_DISPON, ST7789_TIMEOUT_CMD_MS);

    if (ret == 0) {
        if (priv->bl_pwm_dev) {
            uint32_t duty = (uint32_t)priv->bl_brightness * 1023U / 255U;
            ret = device_ioctl(priv->bl_pwm_dev, PWM_CMD_SET_DUTY, &duty, sizeof(duty), ST7789_TIMEOUT_IOCTL_MS);
        } else if (priv->bl_pin >= 0) {
            gpio_level_arg_t level = {
                .pin = priv->bl_pin,
                .level = (priv->bl_brightness > 0) == priv->bl_active_high ? 1 : 0,
            };
            ret = device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level, sizeof(level), ST7789_TIMEOUT_IOCTL_MS);
        }
    }

    osal_mutex_unlock(priv->priv_lock);
    return ret;
}

static int st7789_close(device_t* dev)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    if (priv->display_hdr.magic != DISPLAY_IF_MAGIC) return VFS_ERR_INVAL;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;
    (void)write_cmd(priv, CMD_DISPOFF, ST7789_TIMEOUT_POWEROFF_MS);
    if (priv->bl_pwm_dev) {
        uint32_t off_duty = priv->bl_active_high ? 0U : 1023U;
        (void)device_ioctl(priv->bl_pwm_dev, PWM_CMD_SET_DUTY, &off_duty, sizeof(off_duty), ST7789_TIMEOUT_IOCTL_MS);
    } else if (priv->bl_pin >= 0) {
        gpio_level_arg_t bl_off = { .pin = priv->bl_pin, .level = priv->bl_active_high ? 0 : 1 };
        (void)device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &bl_off, sizeof(bl_off), ST7789_TIMEOUT_IOCTL_MS);
    }
    osal_mutex_unlock(priv->priv_lock);
    return 0;
}

static int st7789_suspend(device_t* dev)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    if (priv->display_hdr.magic != DISPLAY_IF_MAGIC) return VFS_ERR_INVAL;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;
    int ret = write_cmd(priv, CMD_SLPIN, ST7789_TIMEOUT_CMD_MS);
    if (ret == 0) osal_delay_ms(ST7789_TIMEOUT_SLEEP_MS);
    if (ret == 0) {
        if (priv->bl_pwm_dev) {
            uint32_t off_duty = priv->bl_active_high ? 0U : 1023U;
            ret = device_ioctl(priv->bl_pwm_dev, PWM_CMD_SET_DUTY, &off_duty, sizeof(off_duty), ST7789_TIMEOUT_IOCTL_MS);
        } else if (priv->bl_pin >= 0) {
            gpio_level_arg_t bl_off = { .pin = priv->bl_pin, .level = priv->bl_active_high ? 0 : 1 };
            ret = device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &bl_off, sizeof(bl_off), ST7789_TIMEOUT_IOCTL_MS);
        }
    }
    osal_mutex_unlock(priv->priv_lock);
    return ret;
}

static int st7789_resume(device_t* dev)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    if (priv->display_hdr.magic != DISPLAY_IF_MAGIC) return VFS_ERR_INVAL;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;
    int ret = write_cmd(priv, CMD_SLPOUT, ST7789_TIMEOUT_CMD_MS);
    if (ret == 0) osal_delay_ms(ST7789_TIMEOUT_WAKE_MS);
    if (ret == 0) ret = write_cmd(priv, CMD_NORON, ST7789_TIMEOUT_CMD_MS);
    if (ret == 0) ret = write_cmd(priv, CMD_DISPON, ST7789_TIMEOUT_CMD_MS);
    if (ret == 0) {
        if (priv->bl_pwm_dev) {
            uint32_t duty = (uint32_t)priv->bl_brightness * 1023U / 255U;
            ret = device_ioctl(priv->bl_pwm_dev, PWM_CMD_SET_DUTY, &duty, sizeof(duty), ST7789_TIMEOUT_IOCTL_MS);
        } else if (priv->bl_pin >= 0) {
            gpio_level_arg_t level = {
                .pin = priv->bl_pin,
                .level = (priv->bl_brightness > 0) == priv->bl_active_high ? 1 : 0,
            };
            ret = device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level, sizeof(level), ST7789_TIMEOUT_IOCTL_MS);
        }
    }
    osal_mutex_unlock(priv->priv_lock);
    return ret;
}

static int st7789_probe(device_t* dev)
{
    int dc_pin = -1, rst_pin = -1;
    int width = 0, height = 0, bl_active = 1;
    int ret = 0;
    st7789_priv_t* priv = NULL;
    int pool_idx = -1;

    if (device_get_prop_int(dev, "dc", &dc_pin) != 0 ||
        device_get_prop_int(dev, "width", &width) != 0 ||
        device_get_prop_int(dev, "height", &height) != 0 ||
        dc_pin < 0 || width <= 0 || height <= 0) {
        DRV_LOGE(kTag, "missing required display config");
        ret = VFS_ERR_INVAL;
        goto err_pool;
    }
    device_get_prop_int(dev, "reset", &rst_pin);
    device_get_prop_int(dev, "bl_active_high", &bl_active);

    pool_idx = osal_pool_claim(s_st7789_used, ST7789_COUNT);
    if (pool_idx >= 0) {
        priv = &s_st7789_pool[pool_idx];
        memset(priv, 0, sizeof(*priv));
        priv->pool_idx = pool_idx;
        if (osal_mutex_create_static(&priv->priv_lock, priv->priv_lock_storage, sizeof(priv->priv_lock_storage)) != 0) {
            ret = VFS_ERR_NOMEM;
            goto err_pool;
        }
        priv->display_hdr.magic = DISPLAY_IF_MAGIC;
        priv->display_hdr.ops   = &st7789_display_ops;
    }
    if (!priv) {
        ret = VFS_ERR_NOMEM;
        goto err_pool;
    }

    priv->width = width;
    priv->height = height;
    priv->bl_pin = -1;
    priv->bl_active_high = bl_active;
    priv->spi_chunk = ST7789_DEFAULT_SPI_CHUNK;
    priv->line_buf = s_st7789_line_buf[pool_idx];
    priv->line_buf_len = ST7789_LINE_BUF_SIZE;
    priv->bl_brightness = 255;

    priv->spi_dev = device_get_parent(dev);
    if (!priv->spi_dev) {
        DRV_LOGE(kTag, "missing SPI parent");
        ret = VFS_ERR_IO;
        goto err_pool;
    }
    priv->gpio_dev = device_get_phandle_dev(dev, "gpio");
    if (!priv->gpio_dev) {
        ret = VFS_ERR_DEFER;
        goto err_pool;
    }

    int spi_chunk = (int)ST7789_DEFAULT_SPI_CHUNK;
    if (device_get_prop_int(priv->spi_dev, "max_transfer_sz", &spi_chunk) == 0 && spi_chunk > 0) {
        priv->spi_chunk = (size_t)spi_chunk;
    }

    priv->bl_pwm_dev = device_get_phandle_dev(dev, "backlight");
    if (!priv->bl_pwm_dev) {
        device_get_prop_int(dev, "backlight", &priv->bl_pin);
    }

    priv->gpio_dc.pin = dc_pin;
    priv->gpio_dc.mode = HAL_GPIO_MODE_OUTPUT;
    if (device_ioctl(priv->gpio_dev, GPIO_CMD_CONFIG, &priv->gpio_dc, sizeof(priv->gpio_dc), ST7789_TIMEOUT_IOCTL_MS) != 0) {
        ret = VFS_ERR_IO;
        goto err_pool;
    }

    if (rst_pin >= 0) {
        priv->gpio_rst.pin = rst_pin;
        priv->gpio_rst.mode = HAL_GPIO_MODE_OUTPUT;
        if (device_ioctl(priv->gpio_dev, GPIO_CMD_CONFIG, &priv->gpio_rst, sizeof(priv->gpio_rst), ST7789_TIMEOUT_IOCTL_MS) != 0) {
            ret = VFS_ERR_IO;
            goto err_pool;
        }
    }

    if (!priv->bl_pwm_dev && priv->bl_pin >= 0) {
        hal_gpio_config_t bl_cfg = { .pin = priv->bl_pin, .mode = HAL_GPIO_MODE_OUTPUT };
        if (device_ioctl(priv->gpio_dev, GPIO_CMD_CONFIG, &bl_cfg, sizeof(bl_cfg), ST7789_TIMEOUT_IOCTL_MS) != 0) {
            ret = VFS_ERR_IO;
            goto err_pool;
        }
    }

    device_set_priv(dev, priv);
    device_set_subsys_priv(dev, &priv->display_hdr);
    dev->ops = &st7789_fops;
    DRV_LOGI(kTag, "probed: %dx%d ST7789 (DC=%d,RST=%d)",
             width, height, dc_pin, rst_pin);
    return 0;

err_pool:
    if (pool_idx >= 0) osal_pool_release(s_st7789_used, ST7789_COUNT, pool_idx);
    return ret;
}

static int st7789_remove(device_t* dev)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (priv) {
        if (priv->bl_pwm_dev) {
            uint32_t off_duty = priv->bl_active_high ? 0U : 1023U;
            (void)device_ioctl(priv->bl_pwm_dev, PWM_CMD_SET_DUTY, &off_duty, sizeof(off_duty), ST7789_TIMEOUT_IOCTL_MS);
        } else if (priv->gpio_dev && priv->bl_pin >= 0) {
            gpio_level_arg_t bl_off = { .pin = priv->bl_pin, .level = priv->bl_active_high ? 0 : 1 };
            (void)device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &bl_off, sizeof(bl_off), ST7789_TIMEOUT_IOCTL_MS);
        }
        (void)write_cmd(priv, CMD_DISPOFF, ST7789_TIMEOUT_POWEROFF_MS);
        osal_mutex_destroy(priv->priv_lock);
        device_ops_unregister(dev);
        osal_pool_release(s_st7789_used, ST7789_COUNT, priv->pool_idx);
    }
    return 0;
}

DRIVER_REGISTER(st7789, "sitronix,st7789", st7789_probe, st7789_remove);

static int st7789_init(device_t* dev)
{
    (void)dev;
    return 0;
}

static int st7789_get_info(device_t* dev, display_info_t* info)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv || !info) return VFS_ERR_INVAL;
    info->width = priv->width;
    info->height = priv->height;
    return 0;
}

static int st7789_fill_rect(device_t* dev, int x, int y, int w, int h, uint16_t color, uint32_t timeout_ms)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    if (clip_rect(priv, &x, &y, &w, &h) != 0) return VFS_ERR_INVAL;
    if ((size_t)w * 2U > priv->line_buf_len) return VFS_ERR_NOSPC;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;

    int ret = set_window(priv, x, y, w, h, timeout_ms);
    if (ret == 0) ret = write_cmd(priv, CMD_RAMWR, timeout_ms);
    if (ret == 0) hal_gpio_set_level_fast(priv->gpio_dc.pin, 1);
    if (ret == 0) {
        uint8_t hi = (uint8_t)(color >> 8);
        uint8_t lo = (uint8_t)(color & 0xFF);
        for (int i = 0; i < w; i++) {
            priv->line_buf[i * 2] = hi;
            priv->line_buf[i * 2 + 1] = lo;
        }
        size_t row_len = (size_t)w * 2U;
        for (int row = 0; row < h; row++) {
            ret = spi_write_chunked(priv, priv->line_buf, row_len, timeout_ms);
            if (ret != 0) break;
        }
    }

    osal_mutex_unlock(priv->priv_lock);
    return ret;
}

static int st7789_fill_screen(device_t* dev, uint16_t color, uint32_t timeout_ms)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;
    return st7789_fill_rect(dev, 0, 0, priv->width, priv->height, color, timeout_ms);
}

static int st7789_draw_bitmap(device_t* dev, int x, int y, int w, int h, const uint8_t* data, uint32_t timeout_ms)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv || !data || !rect_in_bounds(priv, x, y, w, h)) return VFS_ERR_INVAL;

    size_t pixels = (size_t)w * (size_t)h;
    if (h != 0 && pixels / (size_t)h != (size_t)w) return VFS_ERR_INVAL;
    if (pixels > SIZE_MAX / 2U) return VFS_ERR_NOSPC;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;

    int ret = set_window(priv, x, y, w, h, timeout_ms);
    if (ret == 0) ret = write_cmd(priv, CMD_RAMWR, timeout_ms);
    if (ret == 0) hal_gpio_set_level_fast(priv->gpio_dc.pin, 1);
    if (ret == 0) {
        ret = spi_write_chunked(priv, data, pixels * 2U, timeout_ms);
    }

    osal_mutex_unlock(priv->priv_lock);
    return ret;
}

static int st7789_set_backlight(device_t* dev, uint8_t brightness)
{
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (!priv) return VFS_ERR_INVAL;

    if (osal_mutex_lock(priv->priv_lock, 500) != 0) return VFS_ERR_TIMEOUT;

    int ret;
    if (priv->bl_pwm_dev) {
        uint32_t duty = (uint32_t)brightness * 1023U / 255U;
        ret = device_ioctl(priv->bl_pwm_dev, PWM_CMD_SET_DUTY, &duty, sizeof(duty), ST7789_TIMEOUT_IOCTL_MS);
    } else if (priv->bl_pin >= 0) {
        gpio_level_arg_t level = {
            .pin = priv->bl_pin,
            .level = (brightness > 0) == priv->bl_active_high ? 1 : 0,
        };
        ret = device_ioctl(priv->gpio_dev, GPIO_CMD_SET_LEVEL, &level, sizeof(level), ST7789_TIMEOUT_IOCTL_MS);
    } else {
        ret = VFS_ERR_NOSPC;
    }

    if (ret == 0) {
        priv->bl_brightness = brightness;
    }

    osal_mutex_unlock(priv->priv_lock);
    return ret;
}

static int st7789_write_ram(device_t* dev, int x, int y, int w, int h, const uint16_t* pixels, uint32_t timeout_ms)
{
    return st7789_draw_bitmap(dev, x, y, w, h, (const uint8_t*)pixels, timeout_ms);
}

/* ── display_ops 表: 所有回调函数均已定义, 可安全取址 ── */
static const struct display_ops st7789_display_ops = {
    .fill_rect    = st7789_fill_rect,
    .fill_screen  = st7789_fill_screen,
    .draw_bitmap  = st7789_draw_bitmap,
    .write_ram    = st7789_write_ram,
    .set_backlight = st7789_set_backlight,
    .get_info     = st7789_get_info,
};
