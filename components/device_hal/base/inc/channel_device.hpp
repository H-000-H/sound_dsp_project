#pragma once

class ChannelDevice
{
public:
    virtual void init() = 0;

protected:
    virtual ~ChannelDevice() = default;
    virtual void set_config() = 0;
};
