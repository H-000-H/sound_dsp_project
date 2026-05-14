#include "blue.hpp"
#if CONFIG_ENABLE_DEVICE_HAL_BLUE

extern "C"
{
#include "bsp_blue.h"
}

#include "esp_log.h"

#include <cstddef>

namespace
{
void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    if(event==ESP_A2D_CONNECTION_STATE_EVT/*连接状态改变*/)
    {
        switch(param->conn_stat.state)
        {
            case ESP_A2D_CONNECTION_STATE_CONNECTED:
            {
                ESP_LOGI("connect","ESP_A2D_CONNECTION_STATE_CONNECTED");
                break;
            }
            case ESP_A2D_CONNECTION_STATE_CONNECTING:
            {
                break;
            }
            case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            {
                break;
            }
            case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            {
                break;
            }
            default: break;
        }
    }
}

int32_t source_data_cb(uint8_t *buf, int32_t len)
{
    (void)buf;
    return 0;
}

void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    (void)event;
    (void)param;
    // if(event==ESP_BT_GAP_DISC_RES_EVT)
    // {
    //     esp_bd_addr_t remote_bda = {0x20, 0x3B, 0x34, 0x6B, 0x60, 0x9F};
    //     esp_a2d_source_connect(remote_bda);
    // }
}
}

struct Blue::Impl
{
    bsp_blue_handle_t handler = {};
};

Blue::Blue() : m_impl(new Impl)
{
}

Blue::~Blue()
{
    delete m_impl;
}

void Blue::set_config()
{
    auto& handler = m_impl->handler;
    handler.bt_cfg =BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    handler.bluebooth_name = "audio box";
    handler.MODE[0] =ESP_BT_MODE_BLE;
    handler.MODE[1] =ESP_BT_MODE_CLASSIC_BT;
    handler.a2d_cb=a2d_cb;
    handler.a2d_source_data_cb=source_data_cb;
    handler.gap_cb =gap_cb;
}

void Blue::init()
{
    set_config();
    bsp_blue_init(&m_impl->handler);
}

void Blue::connect(const uint8_t remote_bda[6])
{
    if(!remote_bda)
    {
        return;
    }

    esp_bd_addr_t addr = {};
    for(size_t i = 0; i < sizeof(addr); ++i)
    {
        addr[i] = remote_bda[i];
    }
    esp_a2d_source_connect(addr);
}
#endif
