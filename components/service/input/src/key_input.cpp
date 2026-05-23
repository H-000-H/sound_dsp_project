#include "key_input.hpp"

#include "capability/input_engine.hpp"
#include "device.h"

/* ── Input capability (module-level, init on first use) ── */
static input_engine_t s_input_eng;
static bool s_input_inited = false;

static input_engine_t* get_input()
{
    if (!s_input_inited)
    {
        input_engine_init_struct(&s_input_eng);
        s_input_eng.init(&s_input_eng);
        s_input_inited = true;
    }
    return &s_input_eng;
}

/* ── 按键 GPIO 缓存 ── */
static int s_gpio_next  = -1;
static int s_gpio_prev  = -1;
static int s_gpio_enter = -1;
static int s_gpio_esc   = -1;
static bool s_gpio_inited = false;

static void init_gpio_map()
{
    if (s_gpio_inited) return;
    s_gpio_inited = true;

    device_t* btn = device_find("buttons0");
    if (!btn) return;

    device_get_prop_int(btn, "next_pin",  &s_gpio_next);
    device_get_prop_int(btn, "prev_pin",  &s_gpio_prev);
    device_get_prop_int(btn, "enter_pin", &s_gpio_enter);
    device_get_prop_int(btn, "esc_pin",   &s_gpio_esc);
}

int KeyInput::getGpioNext()  { init_gpio_map(); return s_gpio_next; }
int KeyInput::getGpioPrev()  { init_gpio_map(); return s_gpio_prev; }
int KeyInput::getGpioEnter() { init_gpio_map(); return s_gpio_enter; }
int KeyInput::getGpioEsc()   { init_gpio_map(); return s_gpio_esc; }

KeyInput& KeyInput::getInstance()
{
    static KeyInput instance;
    return instance;
}

void KeyInput::process(uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (!m_dev)
    {
        m_dev = device_find("buttons0");
        if (!m_dev) return;
    }

    input_engine_t* eng = get_input();
    input_state_t state;
    int ret = eng->scan(eng, &state, 1);
    if (ret > 0 && state.event == INPUT_EVENT_PRESS)
    {
        m_last_gpio = state.gpio_pin;
    }
    else if (ret > 0 && state.event == INPUT_EVENT_RELEASE)
    {
        m_last_gpio = -1;
    }
}
