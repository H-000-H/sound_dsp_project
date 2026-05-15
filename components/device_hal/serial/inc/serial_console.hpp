#pragma once

#include <cstdint>
#include <cstddef>

#include "freertos/FreeRTOS.h"
#include "buffer.h"

/**
 * @brief 串口控制台管理器，供串口调试 App 使用（通过 UART0 读取 USB Serial/JTAG 端口）
 *
 * 封装 ESP-IDF UART 驱动，使用环形缓冲区存储传入数据，
 * 供 LVGL 定时器轮询以更新显示
 */
class SerialConsole
{
public:
    static SerialConsole& get_instance();

    /** 使用指定的引脚/波特率配置初始化 UART
     *  如果已初始化，则先反初始化 */
    void init(int tx_pin, int rx_pin, int baud_rate);

    /** 反初始化 UART，释放资源 */
    void deinit();

    /** 向 UART 写入原始数据 */
    void write(const uint8_t* data, size_t len);
    void write(const char* str);

    /** 从内部环形缓冲区读取待处理字节（非阻塞）
     *  返回实际读取的字节数 */
    size_t read(uint8_t* buf, size_t max_len);

    /** 当前缓冲区中可读取的字节数 */
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

    /* UART 接收任务 — 读取硬件数据，推入环形缓冲区 */
    static void uart_rx_task(void* param);

    static constexpr size_t RING_BUF_SIZE = 4096;

    bool    m_active = false;
    bool    m_connected = false;   /* 是否检测到 USB 主机连接 */
    int m_baud_rate = 115200;
    int m_uart_num;  /* 未使用（USB Serial/JTAG 直接读取） */

    mutable FIFO_Type_Def m_fifo;
    Fifo_Data_type* m_fifo_buf = nullptr;   /* init() 中从 PSRAM 分配 */

    TaskHandle_t m_rx_task = nullptr;
};