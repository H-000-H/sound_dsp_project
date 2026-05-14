#include "serial_console.hpp"

#include <cstring>

#include "hal/usb_serial_jtag_ll.h"

#include "esp_log.h"

static const char* TAG = "SerialConsole";

/* Ring buffer helpers */
static inline size_t ring_used(size_t head, size_t tail, size_t cap)
{
    return (head >= tail) ? (head - tail) : (cap - tail + head);
}

static inline size_t ring_free(size_t head, size_t tail, size_t cap)
{
    return cap - ring_used(head, tail, cap) - 1;
}

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

    /* Read from the built-in USB Serial/JTAG controller directly via HAL.
     * This does NOT install a separate driver, avoiding conflict with
     * the ESP-IDF console driver that also uses USB Serial/JTAG.
     * The console still gets its output, we just also read the RX FIFO. */
    (void)tx_pin; (void)rx_pin;
    m_baud_rate = baud_rate;

    /* Allocate ring buffer from PSRAM to save DRAM */
    if (!m_ring) {
        m_ring = (uint8_t*)heap_caps_calloc(1, RING_BUF_SIZE, MALLOC_CAP_SPIRAM);
        if (!m_ring) {
            ESP_LOGW(TAG, "PSRAM not available, fallback to DRAM");
            m_ring = (uint8_t*)calloc(1, RING_BUF_SIZE);
        }
    }
    m_ring_head = m_ring_tail = 0;

    m_active = true;
    m_connected = true;   /* 初始认为已连接，由 uart_rx_task 检测断开 */

    /* Start polling task */
    xTaskCreatePinnedToCore(uart_rx_task, "usb_rx", 3 * 1024, this, 8, &m_rx_task, 1);

    ESP_LOGI(TAG, "USB Serial/JTAG direct read init OK");
}

void SerialConsole::deinit()
{
    if (!m_active) return;
    m_active = false;

    /* 等待 RX 任务自行退出（最多等 100ms） */
    int wait = 0;
    while (m_rx_task && wait < 10) {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait++;
    }
    if (m_rx_task) {
        vTaskDelete(m_rx_task);
        m_rx_task = nullptr;
    }
    if (m_ring) {
        free(m_ring);
        m_ring = nullptr;
    }
    m_connected = false;
    ESP_LOGI(TAG, "USB Serial/JTAG deinit");
}

void SerialConsole::write(const uint8_t* data, size_t len)
{
    if (!m_active || !m_connected) return;
    size_t written = 0;
    while (written < len) {
        uint32_t chunk = (len - written > 64) ? 64 : (len - written);
        usb_serial_jtag_ll_write_txfifo(data + written, chunk);
        written += chunk;
    }
    usb_serial_jtag_ll_txfifo_flush();
}

void SerialConsole::write(const char* str)
{
    write(reinterpret_cast<const uint8_t*>(str), strlen(str));
}

size_t SerialConsole::read(uint8_t* buf, size_t max_len)
{
    size_t total = 0;
    while (total < max_len && m_ring_tail != m_ring_head) {
        buf[total++] = m_ring[m_ring_tail];
        m_ring_tail  = (m_ring_tail + 1) % RING_BUF_SIZE;
    }
    return total;
}

size_t SerialConsole::available() const
{
    return ring_used(m_ring_head, m_ring_tail, RING_BUF_SIZE);
}

void SerialConsole::poll_connection()
{
    if (!m_active) { m_connected = false; return; }
    /* 不直接访问 USB 寄存器，只通过任务行为推断 */
    if (m_ring_head != m_ring_tail && !m_connected) {
        m_connected = true;
        ESP_LOGI(TAG, "USB connected (RX data)");
    }
}

void SerialConsole::uart_rx_task(void* param)
{
    auto* self = static_cast<SerialConsole*>(param);
    uint8_t tmp[64];
    int rx_idle = 0;

    while (self->m_active) {
        uint32_t count = usb_serial_jtag_ll_read_rxfifo(tmp, sizeof(tmp));
        if (count > 0) {
            rx_idle = 0;
            self->m_connected = true;
            for (uint32_t i = 0; i < count; i++) {
                if (ring_free(self->m_ring_head, self->m_ring_tail, RING_BUF_SIZE) == 0) break;
                self->m_ring[self->m_ring_head] = tmp[i];
                self->m_ring_head = (self->m_ring_head + 1) % RING_BUF_SIZE;
            }
        } else {
            rx_idle++;
            /* 连续 1000 次（10 秒）没数据时检查硬件状态 */
            if (rx_idle > 1000) {
                rx_idle = 0;
                if (!usb_serial_jtag_ll_txfifo_writable() && !usb_serial_jtag_ll_phy_is_pad_enabled()) {
                    self->m_connected = false;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    self->m_rx_task = nullptr;
    vTaskDelete(nullptr);
}