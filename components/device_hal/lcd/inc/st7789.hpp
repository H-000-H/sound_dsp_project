#pragma once
#include "display_device.hpp"
#include "spi_bus.hpp"
#include <cstddef>
enum Rgb565Color
{
     COLOR_BLACK       = 0x0000,
     COLOR_WHITE       = 0xFFFF,
     COLOR_RED         = 0xF800,
     COLOR_GREEN       = 0x07E0,
     COLOR_BLUE        = 0x001F,
     COLOR_YELLOW      = 0xFFE0,
     COLOR_CYAN        = 0x07FF,
     COLOR_MAGENTA     = 0xF81F,
     COLOR_GRAY        = 0x8410,
     COLOR_GRAY_LIGHT  = 0xD6BA,
     COLOR_GRAY_DARK   = 0x4208,
     COLOR_ORANGE      = 0xFD20,
     COLOR_PINK        = 0xF81F,
     COLOR_PURPLE      = 0x8010,
     COLOR_LIME        = 0x07E0,
     COLOR_TEAL        = 0x0410,
     COLOR_NAVY        = 0x0010,
     COLOR_MAROON      = 0x8000,
     COLOR_OLIVE       = 0x8400,
     COLOR_BG_DARK     = 0x18E3,
     COLOR_BG_LIGHT    = 0xFFDF,
     COLOR_ACCENT      = 0xE5FF,
};

/*st7789 7线hal层*/
class St7789Display7Wire : public DisplayDevice
{
private:
    St7789Display7Wire();
    ~St7789Display7Wire();
    struct Impl;
    Impl* m_impl;
public:
    static St7789Display7Wire& get_instance();
    void init() override;
    void clear() override;
    void fill_screen(uint16_t color) override;
    void fill_rect(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color) override;
    void draw_bitmap(uint16_t x_start,
                     uint16_t y_start,
                     uint16_t x_end,
                     uint16_t y_end,
                     const uint16_t* color_data,
                     size_t pixel_count) override;
    void set_config() override;
    void configure_default_panel();
};
