#pragma once

#include <cstdint>
#include <cstddef>

#include "freertos/FreeRTOS.h"

/**
 * @brief UART console manager for the serial debug app (reads USB Serial/JTAG port via UART0).
 *
 * Wraps ESP-IDF UART driver with a ring buffer for incoming data,
 * designed to be polled by a LVGL timer for display updates.
 */
class SerialConsole
{
public:
    static SerialConsole& get_instance();

    /** Initialize UART with the given pin/baud config.
     *  If already initialized, deinits first. */
    void init(int tx_pin, int rx_pin, int baud_rate);

    /** Deinitialize UART and free resources. */
    void deinit();

    /** Write raw data out the UART. */
    void write(const uint8_t* data, size_t len);
    void write(const char* str);

    /** Read pending bytes from the internal ring buffer (non-blocking).
     *  Returns number of bytes actually read. */
    size_t read(uint8_t* buf, size_t max_len);

    /** How many bytes are currently buffered and ready to read. */
    size_t available() const;

    bool is_active() const { return m_active; }
    /** 检测 USB 是否真正与 PC 连接（硬件寄存器检测） */
    bool is_connected() const { return m_connected; }
    int  get_baud_rate() const { return m_baud_rate; }

    /** 强制重新检测连接状态（由 impl 层定时调用） */
    void poll_connection();

private:
    SerialConsole();
    ~SerialConsole();

    SerialConsole(const SerialConsole&) = delete;
    SerialConsole& operator=(const SerialConsole&) = delete;

    /* UART receive task — reads from UART HW, pushes into ring buffer */
    static void uart_rx_task(void* param);

    static constexpr size_t RING_BUF_SIZE = 4096;

    bool    m_active = false;
    bool    m_connected = false;   /* 是否检测到 USB 主机连接 */
    int m_baud_rate = 115200;
    int m_uart_num;  /* unused (USB Serial/JTAG direct read) */

    uint8_t* m_ring = nullptr;   /* allocated from PSRAM in init() */
    size_t  m_ring_head = 0;
    size_t  m_ring_tail = 0;

    TaskHandle_t m_rx_task = nullptr;
};