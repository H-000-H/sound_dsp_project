#if LV_USE_LOTTIE
#include "gear_anim.h"
#include "lottie_gear_data.h"
#include "esp_heap_caps.h"

#define LOTTIE_W 200
#define LOTTIE_H 200
#define LOTTIE_BUF_SIZE (LOTTIE_W * LOTTIE_H * 4)

static struct {
    lv_obj_t*  lottie;
    uint8_t*   buf;
    bool       ready;
} s_gear = {NULL, NULL, false};

static lv_obj_t* s_hidden_parent = NULL;

void gear_anim_preload(void)
{
    if (s_gear.ready) return;
    s_gear.buf = (uint8_t*)heap_caps_malloc(LOTTIE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_gear.buf) return;
    s_hidden_parent = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(s_hidden_parent, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_hidden_parent, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_hidden_parent, 0, 0);
    s_gear.lottie = lv_lottie_create(s_hidden_parent);
    if (!s_gear.lottie)
    {
        heap_caps_free(s_gear.buf); s_gear.buf = NULL; return;
    }
    lv_lottie_set_buffer(s_gear.lottie, LOTTIE_W, LOTTIE_H, s_gear.buf);
    lv_lottie_set_src_data(s_gear.lottie, lottie_gear_json, lottie_gear_json_size);
    lv_obj_add_flag(s_gear.lottie, LV_OBJ_FLAG_HIDDEN);
    s_gear.ready = true;
}

lv_obj_t* gear_anim_attach(lv_obj_t* parent)
{
    if (!s_gear.ready || !s_gear.lottie) return NULL;
    lv_obj_set_parent(s_gear.lottie, parent);
    lv_obj_center(s_gear.lottie);
    lv_obj_remove_flag(s_gear.lottie, LV_OBJ_FLAG_HIDDEN);
    return s_gear.lottie;
}
#else
#include "gear_anim.h"
void gear_anim_preload(void) { (void)0; }
lv_obj_t* gear_anim_attach(lv_obj_t* parent) { (void)parent; return NULL; }
#endif
