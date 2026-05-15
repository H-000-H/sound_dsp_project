#pragma once
#include "app_base.hpp"
#include <cstdint>

/* 串口调试共用常量（暴露给 impl 使用） */
extern const int BAUD_RATES[];
extern const int BAUD_COUNT;
extern const char* TERM_MODES[];
extern const int TERM_MODE_COUNT;


/* 环形缓冲的滚动偏移量（定义在 impl.cpp） */
extern int s_scroll_offset;

/* Layer 2: SerialApp — 串口终端 UI 布局 + 焦点环 + 可重载行为钩子 */
class SerialApp : public AppBase {
public:
    virtual void show() override;
    virtual void hide() override;
    lv_obj_t* screen() const override { return m_screen; }
    const char* app_name() const override { return "串口调试"; }
    const lv_image_dsc_t* app_icon() const override;

protected:
    /* ——— 子类可重载的钩子 ——— */
    virtual void on_baud_change(int idx) {}
    virtual void on_term_mode_change(int idx) {}
    virtual void on_view_up() {}
    virtual void on_view_down() {}
    virtual void on_toggle_pause() {}
    virtual void on_clear() {}

    /* ——— UI 构建 ——— */
    void build_ui();
    void focus_update();
    void update_display();
    void handle_key(uint32_t key);

    /* ——— LVGL 定时器回调（静态成员，可访问 protected 成员） ——— */
    static void nav_timer_cb(lv_timer_t* t);
    static void refr_timer_cb(lv_timer_t* t);

    lv_obj_t* m_screen     = nullptr;
    lv_obj_t* m_output     = nullptr;   /* spangroup 输出区 */
    lv_obj_t* m_baud_val   = nullptr;

    lv_obj_t* m_term_val   = nullptr;
    lv_obj_t* m_view_lbl   = nullptr;
    lv_obj_t* m_clear_lbl  = nullptr;
    lv_obj_t* m_conn_dot   = nullptr;

    lv_timer_t* m_nav_timer = nullptr;
    lv_timer_t* m_refr_timer = nullptr;

    int m_focus      = 0;
    bool m_adjust    = false;
    int m_baud_idx   = 4;     /* 默认 115200 */
    int m_term_mode  = 0;     /* 0=终端, 1=HEX */
    bool m_paused      = false;  /* 暂停自动滚屏，进入查看历史模式 */
    int  m_view_page   = 0;      /* 翻页查看时的页码（0=最新） */
    bool m_connected    = false;

    enum { FOCUS_BAUD, FOCUS_TERM_MODE, FOCUS_VIEW, FOCUS_CLEAR, FOCUS_COUNT };

    static constexpr int HDR_H      = 40;
    static constexpr int TXT_Y      = HDR_H + 4;
    static constexpr int TXT_H      = 128;
    static constexpr int FONT_H     = 16;
    static constexpr int STA_H      = 26;
    static constexpr int STA_Y      = 240 - STA_H;

    static constexpr auto CLR_GREEN     = 0x00FF41;
    static constexpr auto CLR_RED       = 0xFF3B6B;
    static constexpr auto CLR_YELLOW    = 0xFFD700;
    static constexpr auto CLR_ACCENT    = 0x59D0FF;
};

/* Layer 3: SerialImpl — 串口硬件 + 日志钩子 + 环形缓冲 */
class SerialImpl : public SerialApp {
public:
    /* 公共启动方法 — 由 SerialApp::show 调用 */
    void start_serial();
    void stop_serial();

protected:
    void on_baud_change(int idx) override;
    void on_term_mode_change(int idx) override;
    void on_toggle_pause() override;
    void on_clear() override;
    void on_view_up() override;
    void on_view_down() override;

    friend class SerialApp;
};

/* 全局查询：串口调试页面是否为当前活动屏幕 */
bool serial_debug_screen_is_active(void);

