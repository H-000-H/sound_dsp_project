#include "lvgl_main.hpp"
#include "esp_heap_caps.h"
#include "config.hpp"
#include "button.hpp"
#include "ui/screen/inc/status_bar.hpp"
#include "ui/screen/inc/lock_screen.hpp"
#include "ui/screen/inc/gear_page.hpp"

/*====================================================================*/
lv_group_t* main_group;

static void (*s_defer_fn)(void*) = nullptr;
static void* s_defer_arg = nullptr;

void lvgl_defer(void (*fn)(void*), void* arg)
{
    s_defer_fn   = fn;
    s_defer_arg  = arg;
}
/*====================================================================*/

static void disp_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    DisplayDevice* screen = factory_config::screen::get_device();
    if (screen)
    {
        uint32_t width  = (area->x2 - area->x1) + 1;
        uint32_t height = (area->y2 - area->y1) + 1;
        screen->draw_bitmap(area->x1, area->y1, area->x2, area->y2,
                           (uint16_t*)px_map, (width * height));
    }
    lv_disp_flush_ready(disp);
}

static void* lvgl_alloc_buf(size_t sz)
{
    /* 优先内部 RAM（避免 PSRAM cache 问题导致花屏）*/
    uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    if (USE_LVGL_PRAM) caps = MALLOC_CAP_SPIRAM;
    void* p = heap_caps_malloc(sz, caps);
    if (!p && !USE_LVGL_PRAM) p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!p) p = heap_caps_malloc(sz, MALLOC_CAP_DMA);
    return p;
}

static void lvgl_display_init()
{
    lv_display_t* display = lv_display_create(screen_width, screen_height);
    size_t byte_size = ((screen_width * screen_height) / 2) * RGB565_BYTE;

#if USE_SINGE_BUFFER && USE_DOUBLE_BUFFER
    #error "not open USE_SINGE_BUFFER&&USE_DOUBLE_BUFFER please use one "
#endif

#if USE_SINGE_BUFFER
    void* buf1 = lvgl_alloc_buf(byte_size);
    lv_display_set_buffers(display, buf1, nullptr, byte_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

#if USE_DOUBLE_BUFFER
    void* buf1 = lvgl_alloc_buf(byte_size);
    void* buf2 = lvgl_alloc_buf(byte_size);
    lv_display_set_buffers(display, buf1, buf2, byte_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    lv_display_set_flush_cb(display, disp_flush_cb);
}

static void lv_tick_cb(void* arg)
{
    (void)arg;
    lv_tick_inc(2);
}

static void lv_tick_init()
{
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = lv_tick_cb;
    timer_args.name = "lvgl_tick";
    static esp_timer_handle_t timer_handle = nullptr;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_periodic(timer_handle, 2000);
}

/* 按键映射表 */
static const struct { int gpio; uint32_t lv_key; } s_key_map[] = {
    {CONFIG_LVGL_KEY_NEXT_GPIO,  LV_KEY_NEXT},
    {CONFIG_LVGL_KEY_PREV_GPIO,  LV_KEY_PREV},
    {CONFIG_LVGL_KEY_ENTER_GPIO, LV_KEY_ENTER},
    {CONFIG_LVGL_KEY_ESC_GPIO,   LV_KEY_ESC},
};

static void key_gpio_init(void)
{
    const uint32_t gpio_pins[] = 
    {
        (uint32_t)CONFIG_LVGL_KEY_NEXT_GPIO,
        (uint32_t)CONFIG_LVGL_KEY_PREV_GPIO,
        (uint32_t)CONFIG_LVGL_KEY_ENTER_GPIO,
        (uint32_t)CONFIG_LVGL_KEY_ESC_GPIO,
    };
    size_t count = sizeof(gpio_pins) / sizeof(gpio_pins[0]);
    Button::get_instance().init(gpio_pins, count);
}

/* 长按自动重复 */
#define KEY_REPEAT_DELAY_MS    400u
#define KEY_REPEAT_INTERVAL_MS 100u

enum RepeatPhase { REPEAT_IDLE, REPEAT_WAIT_RELEASE, REPEAT_WAIT_PRESS };

static void key_board_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    static bool was_pressed = false;
    static uint32_t last_key = 0;
    static uint32_t press_start_ms = 0;
    static uint32_t last_repeat_ms = 0;
    static int repeat_phase = REPEAT_IDLE;

    volatile int gpio = Button::get_instance().get_pressed_gpio();

    if (gpio >= 0)
    {
        uint32_t key = 0;
        for (size_t i = 0; i < sizeof(s_key_map) / sizeof(s_key_map[0]); i++)
        {
            if (s_key_map[i].gpio == gpio)
            {
                key = s_key_map[i].lv_key; break;
            }
        }
        if (key)
        {
            uint32_t now = lv_tick_get();

            if (!was_pressed)
            {
                data->state = LV_INDEV_STATE_PRESSED;
                data->key = key;
                press_start_ms = now;
                last_repeat_ms = now;
                last_key = key;
                was_pressed = true;
                repeat_phase = REPEAT_IDLE;
                return;
            }

            if ((key == LV_KEY_ESC || key == LV_KEY_ENTER) &&(now - press_start_ms) >= KEY_REPEAT_DELAY_MS)
            {
                if (repeat_phase == REPEAT_IDLE)
                {
                    repeat_phase = REPEAT_WAIT_PRESS;
                    last_repeat_ms = now - KEY_REPEAT_INTERVAL_MS + 5u;
                    data->state = LV_INDEV_STATE_RELEASED;
                    data->key = key;
                    last_key = key;
                    return;
                }
                if ((now - last_repeat_ms) >= KEY_REPEAT_INTERVAL_MS)
                {
                    last_repeat_ms = now;
                    if (repeat_phase == REPEAT_WAIT_PRESS)
                    {
                        repeat_phase = REPEAT_WAIT_RELEASE;
                        data->state = LV_INDEV_STATE_PRESSED;
                    } 
                    else 
                    {
                        repeat_phase = REPEAT_WAIT_PRESS;
                        data->state = LV_INDEV_STATE_RELEASED;
                    }
                    data->key = key;
                    last_key = key;
                    return;
                }
            }

            data->state = (repeat_phase == REPEAT_WAIT_PRESS)
                          ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            data->key = last_key;
            return;
        }
    }

    if (was_pressed)
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
        was_pressed = false;
        repeat_phase = REPEAT_IDLE;
    } 
    else 
    {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = 0;
    }
}

static void lv_indev_init(lv_display_t* display)
{
    lv_indev_t* indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_display(indev_keypad, display);
    lv_indev_set_read_cb(indev_keypad, key_board_cb);

    main_group = lv_group_create();
    lv_indev_set_group(indev_keypad, main_group);
    lv_group_set_default(main_group);
}

void lvgl_main()
{
    DisplayDevice* screen = factory_config::screen::get_device();
    if (screen) screen->init();

    lvgl_mutex_init();
    lv_tick_init();
    lvgl_display_init();
    key_gpio_init();

    lv_display_t* display = lv_display_get_default();
    lv_indev_init(display);

    lvgl_mutex_lock();
    /* 预解析齿轮动画（在 WDT 注册前执行）*/
    gear_anim_preload();
    /* 初始化状态栏 + 锁屏 */
    status_bar_init();
    lock_screen_show();
    lvgl_mutex_unlock();

    while (true)
    {
        Button::get_instance().process(0);

        lvgl_mutex_lock();
        lv_timer_handler();
        lvgl_mutex_unlock();

        if (s_defer_fn)
        {
            auto fn  = s_defer_fn;
            auto arg = s_defer_arg;
            s_defer_fn  = nullptr;
            s_defer_arg = nullptr;
            fn(arg);
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

