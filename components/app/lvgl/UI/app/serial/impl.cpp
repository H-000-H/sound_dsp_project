#include "serial_app.hpp"
#include "serial_console.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include <cstdarg>
#include "esp_attr.h"



extern "C" { LV_FONT_DECLARE(lv_font_custom_16); }

SerialImpl g_serial_impl;

/* ==================================================================== */
/*  Ring buffer: stores ESP-IDF logs and USB data into s_stored         */
/* ==================================================================== */

static constexpr int MAX_STORED = 512;
static constexpr int LINE_SZ    = 128;

struct StoredLine {
    uint32_t color;
    char     text[LINE_SZ];
};

static EXT_RAM_BSS_ATTR StoredLine s_stored[MAX_STORED];
static int s_stored_head = 0;
static int s_stored_tail = 0;

/* Scroll offset: 0 = latest, positive = scroll back N lines */
int s_scroll_offset = 0;

/* Tracks last rendered ring state to avoid unnecessary redraws */
static int s_last_rendered_head = -1;

static int s_stored_count()
{
    if (s_stored_head >= s_stored_tail)
        return s_stored_head - s_stored_tail;
    return MAX_STORED - s_stored_tail + s_stored_head;
}

/* Get line at ring index (0 = oldest) */
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

/* ESP-IDF log hook -> push_line(color) + forward to USB TX */
static int log_vprintf(const char* fmt, va_list args)
{
    static char buf[256];
    int ret = vsnprintf(buf, sizeof(buf), fmt, args);
    if (ret > 0) {
        /* Strip trailing newline */
        int len = ret;
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            len--;

        /* Color by log level prefix char (format: "I (12345) TAG: msg" / "W ..." / "E ...") */
        uint32_t color = color_by_prefix(buf, len);
        if (len > 0)
            push_line(color, buf, len);
    }
    /* Forward to USB Serial/JTAG TX so PC monitor receives it too */
    /* Forward to USB Serial/JTAG TX so PC monitor receives it too */
    auto& console = SerialConsole::get_instance();
    if (console.is_active() && console.is_connected()) {
        console.write((const uint8_t*)buf, ret);
    }
    return ret;
}

/* ==================================================================== */
/*  LVGL spangroup rendering                                            */
/* ==================================================================== */
void SerialApp::refr_timer_cb(lv_timer_t* t)
{
    auto* app = static_cast<SerialApp*>(lv_timer_get_user_data(t));
    if (!app || !app->m_output) return;
    if (lv_screen_active() != app->screen()) return;

    /* 轮询 USB 连接状态 */
    auto& console = SerialConsole::get_instance();
    if (console.is_active()) {
        console.poll_connection();
        bool was_connected = app->m_connected;
        app->m_connected = console.is_connected();
        if (was_connected != app->m_connected) {
            app->update_display();
        }
    }
    /* Read USB RX data from SerialConsole -> s_stored */
    if (console.is_active()) {
        uint8_t buf[256];
        size_t n = console.read(buf, sizeof(buf) - 1);
        if (n > 0) {
            if (app->m_term_mode == 1) {
                /* HEX 模式：将收到的字节按固定宽度格式化为十六进制文本 */
                static uint8_t s_hex_buf[16];
                static int s_hex_cnt = 0;
                for (size_t i = 0; i < n; i++) {
                    s_hex_buf[s_hex_cnt++] = buf[i];
                    if (s_hex_cnt == 16) {
                        char hex_line[LINE_SZ];
                        int pos = 0;
                        for (int j = 0; j < 16; j++) {
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
                for (size_t i = 0; i < n; i++) {
                    if (buf[i] == '\n' || buf[i] == '\r') {
                        if ((int)i > line_start) {
                            int seg_len = (int)(i - line_start);
                            uint32_t seg_color = color_by_prefix((const char*)(buf + line_start), seg_len);
                            if (seg_color == 0x59D0FF && (buf[line_start] < 'A' || buf[line_start] > 'Z'))
                                seg_color = last_color;
                            else
                                last_color = seg_color;
                            push_line(seg_color, (const char*)(buf + line_start), seg_len);
                        }
                        line_start = (int)(i + 1);
                    }
                }
                if (line_start < (int)n) {
                    int seg_len = (int)(n - line_start);
                    uint32_t seg_color = color_by_prefix((const char*)(buf + line_start), seg_len);
                    if (seg_color == 0x59D0FF && (buf[line_start] < 'A' || buf[line_start] > 'Z'))
                        seg_color = last_color;
                    push_line(seg_color, (const char*)(buf + line_start), seg_len);
                }
            }
        }
    }

    int head = s_stored_head;
    /* No new data and scroll hasn't changed, skip render */
    if (head == s_last_rendered_head && s_scroll_offset == 0 && s_last_rendered_head >= 0) return;

    /* Delete all old spans */
    uint32_t old_cnt = lv_spangroup_get_span_count(app->m_output);
    for (uint32_t i = 0; i < old_cnt; i++) {
        lv_span_t* s = lv_spangroup_get_child(app->m_output, 0);
        if (s) lv_spangroup_delete_span(app->m_output, s);
    }

    int cnt = s_stored_count();
    if (cnt == 0) {
        s_last_rendered_head = head;
        s_scroll_offset = 0;
        lv_spangroup_refresh(app->m_output);
        return;
    }

    /* Determine which lines to show based on scroll offset */
    int max_lines = 10;  /* how many lines fit on screen */
    int total = cnt;

    int start_line;  /* index in ring (0=oldest) of first visible line */
    if (s_scroll_offset == 0) {
        /* Auto-scroll: show latest max_lines lines */
        start_line = (total > max_lines) ? total - max_lines : 0;
    } else {
        /* Manual scroll: show from scroll_offset, but clamp */
        start_line = total - s_scroll_offset - max_lines;
        if (start_line < 0) start_line = 0;
    }

    int show_cnt = total - start_line;
    if (show_cnt > max_lines) show_cnt = max_lines;

    /* Build spans */
    for (int i = 0; i < show_cnt; i++) {
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
/*  impl implementation - called by SerialApp::show()                   */
/* ==================================================================== */
void SerialImpl::start_serial()
{
    auto& console = SerialConsole::get_instance();
    if (!console.is_active())
        console.init(-1, -1, BAUD_RATES[m_baud_idx]);

    /* Hook ESP-IDF log -> s_stored + USB TX */
    esp_log_set_vprintf(log_vprintf);

    m_connected = console.is_active();
    update_display();
}

void SerialImpl::stop_serial()
{
    auto& console = SerialConsole::get_instance();
    if (console.is_active()) {
        console.deinit();
    }
    m_connected = false;
}

/* ==================================================================== */
/*  Behavior hooks                                                      */
/* ==================================================================== */

void SerialImpl::on_baud_change(int idx)
{
    auto& console = SerialConsole::get_instance();
    if (console.is_active()) {
        console.deinit();
        vTaskDelay(pdMS_TO_TICKS(50));
        console.init(-1, -1, BAUD_RATES[idx]);
    }
    m_connected = console.is_active();
    update_display();
}

void SerialImpl::on_term_mode_change(int idx)
{
    (void)idx;
    /* Terminal/HEX is display-format only, no hardware change */
}


void SerialImpl::on_clear()
{
    s_stored_head = s_stored_tail = 0;
    s_last_rendered_head = -1;
    s_scroll_offset = 0;
    if (m_output) {
        lv_spangroup_refresh(m_output);
    }
}

bool serial_debug_screen_is_active(void)
{
    auto* scr = g_serial_impl.screen();
    return scr != nullptr && lv_screen_active() == scr;
}

/* Scroll up/down - called from key handler when FOCUS_SCROLL is in adjust mode */

/* ==================================================================== */
/*  New behavior hooks (pause/view-up/view-down)                        */
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
    if (m_view_page < max_page) {
        m_view_page++;
        s_scroll_offset = m_view_page * max_visible;
        if (s_scroll_offset > cnt - max_visible)
            s_scroll_offset = cnt - max_visible;
    }
    s_last_rendered_head = -1;
}

void SerialImpl::on_view_down()
{
    if (m_view_page > 0) {
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
