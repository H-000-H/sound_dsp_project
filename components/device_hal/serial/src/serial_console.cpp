#include "serial_console.hpp"
#include <cstring>
#include "hal/usb_serial_jtag_ll.h"
#include "esp_log.h"

static const char* TAG = "SerialConsole";

SerialConsole& SerialConsole::get_instance()
{
    static SerialConsole inst;
    return inst;
}

SerialConsole::SerialConsole()  = default;
SerialConsole::~SerialConsole() { deinit(); }

void SerialConsole::init(int tx_pin, int rx_pin, int baud_rate)
{
    if (m_active) deinit();

    (void)tx_pin; (void)rx_pin;
    m_baud_rate = baud_rate;

    m_active = true;
    m_connected = true;

    ESP_LOGI(TAG, "USB Serial/JTAG direct read init OK");
}

void SerialConsole::deinit()
{
    if (!m_active) return;
    m_active = false;
    m_connected = false;
}

void SerialConsole::write(const uint8_t* data, size_t len)
{
    if (!m_active || !m_connected) return;
    size_t pos = 0;
    while (pos < len)
    {
        uint32_t chunk = (len - pos > 64) ? 64 : (len - pos);
        uint32_t n = usb_serial_jtag_ll_write_txfifo(data + pos, chunk);
        pos += n;
        if (n == 0) break;
    }
    if (pos > 0) usb_serial_jtag_ll_txfifo_flush();
}

void SerialConsole::write(const char* str)
{
    write(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

size_t SerialConsole::read(uint8_t* buf, size_t max_len)
{
    /* 从 USB RX FIFO 直接读取，不设独立轮询任务以避免频繁访问 USB 寄存器 */
    return usb_serial_jtag_ll_read_rxfifo(buf, max_len);
}

size_t SerialConsole::available() const
{
    /* 无独立缓存，直接询问硬件 */
    return usb_serial_jtag_ll_rxfifo_data_available() ? 1 : 0;
}

void SerialConsole::poll_connection()
{
    if (!m_active) { m_connected = false; return; }
    /* 收到 RX 数据说明主机已连 */
    if (usb_serial_jtag_ll_rxfifo_data_available() && !m_connected)
    {
        m_connected = true;
        ESP_LOGI(TAG, "USB connected (RX data)");
    }
}
