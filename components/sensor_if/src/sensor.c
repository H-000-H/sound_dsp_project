#include "sensor.h"

int sensor_read_value(device_t* dev, int* value, uint32_t timeout_ms)
{
    if (!dev) return -1;
    sensor_if_priv_t* hdr = (sensor_if_priv_t*)device_get_subsys_priv(dev);
    if (!hdr || hdr->magic != SENSOR_IF_MAGIC || !hdr->ops || !hdr->ops->read_value) return -1;
    return hdr->ops->read_value(dev, value, timeout_ms);
}
