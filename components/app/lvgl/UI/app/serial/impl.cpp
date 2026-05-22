#include "serial_app.hpp"
#include "device.h"
#include "hal_uart.h"
#include "esp_log.h"
#include "lvgl.h"
#include <cstdarg>
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" { LV_FONT_DECLARE(lv_font_custom_16); }

SerialImpl g_serial_impl;

/* ==================================================================== */
/*  环形缓冲区：将 ESP-IDF 日志和 USB 数据存入 s_stored                    */
/* ==================================================================== */

static constexpr int MAX_STORED = 512;
static constexpr int LINE_SZ    = 128;

struct StoredLine
{
    uint32_t color;
    char     text[LINE_SZ];
};

static EXT_RAM_BSS_ATTR StoredLine s_stored[MAX_STORED];
static int s_stored_head = 0;
static int s_stored_tail = 0;

/* 滚动偏移：0 = 最新，正数 = 向后回滚 N 行 */
int s_scroll_offset = 0;

/* 跟踪上次渲染的环形状态，避免不必要的重绘 */
static int s_last_rendered_head = -1;

/* UART 句柄（替换旧的 SerialConsole） */
static hal_uart_t s_uart;
static bool s_uart_inited = false;

static int s_stored_count()
{
    if (s_stored_head >= s_stored_tail)
        return s_stored_head - s_stored_tail;
    return MAX_STORED - s_stored_tail + s_stored_head;
}

/* 获取环形索引处的行（0 = 最早） */
static const StoredLine* get_line(int idx)
{
    if (idx < 0 || idx >= s_stored_count()) return nullptr;
    int real = (s_stored_tail + idx) % MAX_STORED;
    return &s_stored[real];
}

/* 根据文本前缀确定颜色：I=绿, W=黄, E=红, 其他=蓝 */
static uint32_t color_by_prefix(const char* str, int len)
{
    if (len <= 0) return 0x59D0FF;
    if (str[0] == 'I') return 0x00FF41;
    if (str[0] == 'W') return 0xFFD700;
    if (str[0] == 'E') return 0xFF3B6B;
    return 0x59D0FF;
}

static void push_line(uint32_t color, const char* str, int len)
{
    if (len > LINE_SZ - 1) len = LINE_SZ - 1;
    if (len <= 0) return;
    s_stored[s_stored_head].color = color;
    memcpy(s_stored[s_stored_head].text, str, len);
    s_stored[s_stored_head].text[len] = '\0';
    s_stored_head = (s_stored_head + 1) % MAX_STORED;
    if (s_stored_head == s_stored_tail)
        s_stored_tail = (s_stored_tail + 1) % MAX_STORED;
}

/* 指向 esp_log_set_vprintf 之前的原始输出函数（用于 UART TX） */
static vprintf_like_t s_orig_vprintf = nullptr;

/* ESP-IDF 日志钩子 -> 先送原始输出（UART TX），再捕获一份给屏幕显示 */
static int log_vprintf(const char* fmt, va_list args)
{
    int ret = 0;
    if (s_orig_vprintf)
    {
        va_list args_tx;
        va_copy(args_tx, args);
        ret = s_orig_vprintf(fmt, args_tx);
        va_end(args_tx);
    }

    va_list args_cap;
    va_copy(args_cap, args);
    char buf[256];
    int n = vsnprintf(buf, sizeof(buf), fmt, args_cap);
    va_end(args_cap);

    if (n > 0)
    {
        int len = n;
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            len--;

        uint32_t color = color_by_prefix(buf, len);
        if (len > 0)
            push_line(color, buf, len);
    }
    return ret;
}

/* ==================================================================== */
/*  LVGL 渲染                                            */
/* ==================================================================== */
void SerialApp::refr_timer_cb(lv_timer_t* t)
{
    auto* app = static_cast<SerialApp*>(lv_timer_get_user_data(t));
    if (!app || !app->m_output) return;
    if (lv_screen_active() != app->screen()) return;

    /* 从 UART 读取数据 -> s_stored */
    if (s_uart_inited)
    {
        uint8_t buf[256];
        int n = s_uart.read(&s_uart, buf, sizeof(buf) - 1, 100);
        if (n > 0)
        {
            if (app->m_term_mode == 1)
            {
                /* HEX 模式：将收到的字节按固定宽度格式化为十六进制文本 */
                static uint8_t s_hex_buf[16];
                static int s_hex_cnt = 0;
                for (int i = 0; i < n; i++)
                {
                    s_hex_buf[s_hex_cnt++] = buf[i];
                    if (s_hex_cnt == 16)
                    {
                        char hex_line[LINE_SZ];
                        int pos = 0;
                        for (int j = 0; j < 16; j++)
                        {
                            pos += snprintf(hex_line + pos, sizeof(hex_line) - pos, "%02X ", s_hex_buf[j]);
                        }
                        hex_line[pos] = '\0';
                        push_line(0x59D0FF, hex_line, pos);  /* HEX 统一用青色 */
                        s_hex_cnt = 0;
                    }
                }
            } else {
                /* 终端模式：按行解析，根据前缀着色 */
                int line_start = 0;
                uint32_t last_color = 0x59D0FF;
                for (int i = 0; i < n; i++)
                {
                    if (buf[i] == '\n' || buf[i] == '\r')
                    {
                        if (i > line_start)
                        {
                            int seg_len = i - line_start;
                            uint32_t seg_color = color_by_prefix((const char*)(buf + line_start), seg_len);
                            if (seg_color == 0x59D0FF && (buf[line_start] < 'A' || buf[line_start] > 'Z'))
                                seg_color = last_color;
                            else
                                last_color = seg_color;
                            push_line(seg_color, (const char*)(buf + line_start), seg_len);
                        }
                        line_start = i + 1;
                    }
                }
                if (line_start < n)
                {
                    int seg_len = n - line_start;
                    uint32_t seg_color = color_by_prefix((const char*)(buf + line_start), seg_len);
                    if (seg_color == 0x59D0FF && (buf[line_start] < 'A' || buf[line_start] > 'Z'))
                        seg_color = last_color;
                    push_line(seg_color, (const char*)(buf + line_start), seg_len);
                }
            }
        }
    }

    int head = s_stored_head;
    /* 没有新数据且滚动未变，跳过渲染 */
    if (head == s_last_rendered_head && s_scroll_offset == 0 && s_last_rendered_head >= 0) return;

    /* 删除所有旧的 span */
    uint32_t old_cnt = lv_spangroup_get_span_count(app->m_output);
    for (uint32_t i = 0; i < old_cnt; i++)
    {
        lv_span_t* s = lv_spangroup_get_child(app->m_output, 0);
        if (s) lv_spangroup_delete_span(app->m_output, s);
    }

    int cnt = s_stored_count();
    if (cnt == 0)
    {
        s_last_rendered_head = head;
        s_scroll_offset = 0;
        lv_spangroup_refresh(app->m_output);
        return;
    }

    /* 根据滚动偏移决定显示哪些行 */
    int max_lines = 10;  /* 屏幕能容纳的行数 */
    int total = cnt;

    int start_line;  /* 第一条可见行在环形中的索引（0=最早） */
    if (s_scroll_offset == 0)
    {
        /* 自动滚动：显示最新的 max_lines 行 */
        start_line = (total > max_lines) ? total - max_lines : 0;
    } else {
        /* 手动滚动：从 scroll_offset 开始显示，但不超过边界 */
        start_line = total - s_scroll_offset - max_lines;
        if (start_line < 0) start_line = 0;
    }

    int show_cnt = total - start_line;
    if (show_cnt > max_lines) show_cnt = max_lines;

    /* 构建 span 列表 */
    for (int i = 0; i < show_cnt; i++)
    {
        const StoredLine* line = get_line(start_line + i);
        if (!line || line->text[0] == '\0') continue;
        lv_span_t* span = lv_spangroup_add_span(app->m_output);
        /* 每行末尾加换行符确保 span 之间换行 */
        char line_buf[LINE_SZ + 2];
        snprintf(line_buf, sizeof(line_buf), "%s\n", line->text);
        lv_span_set_text(span, line_buf);
        lv_style_set_text_color(lv_span_get_style(span), lv_color_hex(line->color));
        lv_style_set_text_font(lv_span_get_style(span), &lv_font_custom_16);
    }

    lv_spangroup_refresh(app->m_output);
    s_last_rendered_head = head;
}

/* ==================================================================== */
/*  impl 实现 - 由 SerialApp::show() 调用                   */
/* ==================================================================== */

static void uart_init_from_device(void)
{
    hal_uart_init_struct(&s_uart);

    device_t* dev = device_find("uart_debug");

    hal_uart_config_t cfg;
    cfg.tx_pin     = 43;
    cfg.rx_pin     = 44;
    cfg.rts_pin    = -1;
    cfg.cts_pin    = -1;
    cfg.baud_rate  = 115200;
    cfg.data_bits  = 8;
    cfg.stop_bits  = 1;
    cfg.parity     = 0;

    if (dev)
    {
        device_get_prop_int(dev, "tx_pin",    &cfg.tx_pin);
        device_get_prop_int(dev, "rx_pin",    &cfg.rx_pin);
        device_get_prop_int(dev, "baud_rate", &cfg.baud_rate);
        device_get_prop_int(dev, "data_bits", &cfg.data_bits);
        device_get_prop_int(dev, "stop_bits", &cfg.stop_bits);
        device_get_prop_int(dev, "parity",    &cfg.parity);
    }

    s_uart.init(&s_uart, &cfg);
    s_uart_inited = true;
}

void SerialImpl::start_serial()
{
    if (!s_uart_inited)
    {
        uart_init_from_device();
    }

    /* 挂钩 ESP-IDF 日志：原始输出路径不变，另捕获一份给屏幕 */
    s_orig_vprintf = esp_log_set_vprintf(log_vprintf);

    m_connected = s_uart_inited;
    update_display();
}

void SerialImpl::stop_serial()
{
    if (s_uart_inited)
    {
        s_uart.deinit(&s_uart);
        s_uart_inited = false;
    }
    /* 恢复原始输出函数 */
    if (s_orig_vprintf)
    {
        esp_log_set_vprintf(s_orig_vprintf);
        s_orig_vprintf = nullptr;
    }
    m_connected = false;
}

/* ==================================================================== */
/*  行为钩子                                                      */
/* ==================================================================== */

void SerialImpl::on_baud_change(int idx)
{
    if (s_uart_inited)
    {
        s_uart.deinit(&s_uart);
        vTaskDelay(pdMS_TO_TICKS(50));

        device_t* dev = device_find("uart_debug");
        hal_uart_config_t cfg;
        cfg.tx_pin     = 43;
        cfg.rx_pin     = 44;
        cfg.rts_pin    = -1;
        cfg.cts_pin    = -1;
        cfg.baud_rate  = BAUD_RATES[idx];
        cfg.data_bits  = 8;
        cfg.stop_bits  = 1;
        cfg.parity     = 0;

        if (dev)
        {
            device_get_prop_int(dev, "tx_pin",    &cfg.tx_pin);
            device_get_prop_int(dev, "rx_pin",    &cfg.rx_pin);
            device_get_prop_int(dev, "data_bits", &cfg.data_bits);
            device_get_prop_int(dev, "stop_bits", &cfg.stop_bits);
            device_get_prop_int(dev, "parity",    &cfg.parity);
        }

        s_uart.init(&s_uart, &cfg);
    }
    m_connected = s_uart_inited;
    update_display();
}

void SerialImpl::on_term_mode_change(int idx)
{
    (void)idx;
    /* 终端/HEX 模式仅为显示格式，无硬件变更 */
}


void SerialImpl::on_clear()
{
    s_stored_head = s_stored_tail = 0;
    s_last_rendered_head = -1;
    s_scroll_offset = 0;
    if (m_output)
    {
        lv_spangroup_refresh(m_output);
    }
}

bool serial_debug_screen_is_active(void)
{
    auto* scr = g_serial_impl.screen();
    return scr != nullptr && lv_screen_active() == scr;
}

/* 上/下滚动 - FOCUS_SCROLL 处于调整模式时由按键处理器调用 */

/* ==================================================================== */
/*  新行为钩子（暂停/视图上/视图下）                        */
void SerialImpl::on_toggle_pause()
{
    s_scroll_offset = 0;
    m_paused = false;
    m_view_page = 0;
    s_last_rendered_head = -1;
}

void SerialImpl::on_view_up()
{
    int cnt = s_stored_count();
    int max_visible = 10;
    if (cnt <= max_visible) return;
    int max_page = (cnt - 1) / max_visible;
    if (m_view_page < max_page)
    {
        m_view_page++;
        s_scroll_offset = m_view_page * max_visible;
        if (s_scroll_offset > cnt - max_visible)
            s_scroll_offset = cnt - max_visible;
    }
    s_last_rendered_head = -1;
}

void SerialImpl::on_view_down()
{
    if (m_view_page > 0)
    {
        m_view_page--;
        int max_visible = 10;
        s_scroll_offset = m_view_page * max_visible;
        if (s_scroll_offset < 0) s_scroll_offset = 0;
    } else {
        m_paused = false;
        s_scroll_offset = 0;
        m_view_page = 0;
        on_toggle_pause();
    }
    s_last_rendered_head = -1;
}
