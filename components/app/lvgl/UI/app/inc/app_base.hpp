#pragma once
#include "lvgl.h"

class AppBase {
public:
    virtual ~AppBase() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual lv_obj_t* screen() const = 0;

    virtual const char* app_name() const = 0;
    virtual const lv_image_dsc_t* app_icon() const = 0;
};
