#include "st7789.hpp"

extern "C"
{
#include "st7789.h"
}

struct St7789Display7Wire::Impl
{
    bsp_spi_handle* spi_handle = nullptr;
    bsp_lcd_handle_t lcd_config = {};
};

 St7789Display7Wire::St7789Display7Wire() : m_impl(new Impl)
{
}

 St7789Display7Wire::~St7789Display7Wire()
{
    delete m_impl;
}

 St7789Display7Wire& St7789Display7Wire::get_instance()
{
    static St7789Display7Wire instance;
    return instance;
}

 void St7789Display7Wire::init()
{
    configure_default_panel();
    bsp_lcd_init(m_impl->spi_handle, &m_impl->lcd_config);
}

 void St7789Display7Wire::clear()
{
    bsp_lcd_clear(m_impl->spi_handle, &m_impl->lcd_config);
}

 void St7789Display7Wire::fill_screen(uint16_t color)
{
    bsp_lcd_fill_screen(m_impl->spi_handle, color, &m_impl->lcd_config);
}

 void St7789Display7Wire::fill_rect(uint16_t x_start,
                             uint16_t y_start,
                             uint16_t x_end,
                             uint16_t y_end,
                             uint16_t color)
{
    bsp_lcd_fill_rect(m_impl->spi_handle, x_start, y_start, x_end, y_end, color, &m_impl->lcd_config);
}

 void St7789Display7Wire::draw_bitmap(uint16_t x_start,
                               uint16_t y_start,
                               uint16_t x_end,
                               uint16_t y_end,
                               const uint16_t* color_data,
                               size_t pixel_count)
{
    bsp_lcd_draw_bitmap(m_impl->spi_handle,
                        x_start,
                        y_start,
                        x_end,
                        y_end,
                        color_data,
                        pixel_count,
                        &m_impl->lcd_config);
}

 void St7789Display7Wire::set_config()
{
    configure_default_panel();
}

 void St7789Display7Wire::configure_default_panel()
{
    auto& spi = SpiMasterBus::get_instance();
    spi.get_config();
    spi.init();
    m_impl->spi_handle = static_cast<bsp_spi_handle*>(spi.native_handle());

    // 当前屏幕为 240x240 的 ST7789 模组。
    m_impl->lcd_config.LCD_COL_MOD = 0x55;
    m_impl->lcd_config.LCD_SHOW = 0x00;
    m_impl->lcd_config.LCD_WIDTH = 240;
    m_impl->lcd_config.LCD_HEIGHT = 240;
    m_impl->lcd_config.LCD_X_OFFSET = 0;
    m_impl->lcd_config.LCD_Y_OFFSET = 0;
    m_impl->lcd_config.LCD_INIT_COLOR = Rgb565Color::COLOR_RED;
}
