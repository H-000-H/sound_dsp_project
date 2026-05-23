#ifndef BOARD_DRIVER_H
#define BOARD_DRIVER_H

#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Driver 核心 API ── */
int board_driver_probe_all(void);   /* 遍历设备 → 匹配 driver → probe */
int board_driver_remove_all(void);

/* ── 兼容旧入口: 编译期 probe 表不再需要运行时注册 ── */
void board_register_all_drivers(void);

/* ── DRIVER_REGISTER 宏 ──
 * 在驱动 .c 文件中使用:
 *   DRIVER_REGISTER(my_drv, "compat,vendor", my_probe, my_remove);
 * 生成 board_driver_probe_my_drv() / board_driver_remove_my_drv()
 * 由编译期 dtc-lite.py 扫描收录, 运行时无 strcmp 匹配
 */
#define DRIVER_REGISTER(name, compat, probe_fn, remove_fn)        \
    int board_driver_probe_##name(device_t* dev) {                \
        return probe_fn(dev);                                     \
    }                                                             \
    int board_driver_remove_##name(device_t* dev) {               \
        return remove_fn(dev);                                     \
    }

#ifdef __cplusplus
}
#endif

#endif /* BOARD_DRIVER_H */
