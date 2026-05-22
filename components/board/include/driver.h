#ifndef BOARD_DRIVER_H
#define BOARD_DRIVER_H

#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Driver 操作函数表 ── */
typedef struct {
    int (*probe)(device_t* dev);    /* 设备探测 → 初始化硬件 */
    int (*remove)(device_t* dev);   /* 设备移除 → 去初始化 */
} driver_ops_t;

/* ── Driver 描述符 ── */
typedef struct {
    const char*  compatible;        /* 匹配字符串 ex: "sitronix,st7789" */
    driver_ops_t ops;               /* 操作函数 */
} board_driver_t;

/* ── Driver 核心 API ── */
int board_driver_register(board_driver_t* drv);
int board_driver_probe_all(void);   /* 遍历设备 → 匹配 driver → probe */
int board_driver_remove_all(void);

/* ── 外部驱动注册总入口（在 board_driver_list.c 实现） ── */
void board_register_all_drivers(void);

/* ── DRIVER_REGISTER 宏 ──
 * 在驱动 .c 文件中使用:
 *   DRIVER_REGISTER(my_drv, "compat,vendor", my_probe, my_remove);
 * 生成 board_driver_reg_my_drv() 函数, 供 board_driver_list.c 调用
 */
#define DRIVER_REGISTER(name, compat, probe_fn, remove_fn)        \
    static board_driver_t s_drv_##name = {                        \
        .compatible = compat,                                     \
        .ops = { .probe = probe_fn, .remove = remove_fn },        \
    };                                                            \
    int board_driver_reg_##name(void) {                           \
        return board_driver_register(&s_drv_##name);              \
    }

#ifdef __cplusplus
}
#endif

#endif /* BOARD_DRIVER_H */
