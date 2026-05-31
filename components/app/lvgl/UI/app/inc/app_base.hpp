#pragma once
#include "lvgl.h"

class AppBase 
{
public:
    virtual ~AppBase() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual lv_obj_t* screen() const = 0;

    virtual const char* app_name() const = 0;
    virtual const lv_image_dsc_t* app_icon() const = 0;

    /** 彻底释放本页所有 LVGL 资源（屏幕 + 所有子 Widget）。
     *  调用后 screen() 返回 nullptr，下次 show() 会重新 build_ui()。 */
    virtual void on_destroy() {}
};
