#ifndef GEAR_ANIM_H
#define GEAR_ANIM_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_image_dsc_t gear_img;

void gear_anim_preload(void);
lv_obj_t* gear_anim_attach(lv_obj_t* parent);

#ifdef __cplusplus
}
#endif

#endif /* GEAR_ANIM_H */
