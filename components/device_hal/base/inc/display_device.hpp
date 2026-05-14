#pragma once

#include "channel_device.hpp"

#include <cstddef>
#include <cstdint>

class DisplayDevice : public ChannelDevice
{
public:
    virtual void clear() = 0;
    virtual void fill_screen(uint16_t color) = 0;
    virtual void fill_rect(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color) = 0;
    virtual void draw_bitmap(uint16_t x_start,
                             uint16_t y_start,
                             uint16_t x_end,
                             uint16_t y_end,
                             const uint16_t* color_data,
                             size_t pixel_count) = 0;

protected:
    ~DisplayDevice() override = default;
};
