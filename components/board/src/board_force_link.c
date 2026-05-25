// 强制链接器拉入所有 DRIVER_REGISTER 生成的 probe 函数
// 通过 volatile 全局变量使引用逃逸, 编译器无法优化
// .init_array 在链接脚本中被 KEEP → 构造函数代码被保留 → 引用传递到链接器
#include "device.h"

extern int board_driver_probe_st7789(device_t*);
extern int board_driver_probe_max98357a(device_t*);
extern int board_driver_probe_gpio_key(device_t*);
extern int board_driver_probe_ws2812(device_t*);
extern int board_driver_probe_light_sensor(device_t*);

/* volatile 全局变量: 每次写入都是可观察副作用, 编译器无法省略 */
static volatile void* s_fake_ref;

static void __attribute__((constructor, used)) _force_probe_link(void)
{
    s_fake_ref = (void*)board_driver_probe_st7789;
    s_fake_ref = (void*)board_driver_probe_max98357a;
    s_fake_ref = (void*)board_driver_probe_gpio_key;
    s_fake_ref = (void*)board_driver_probe_ws2812;
    s_fake_ref = (void*)board_driver_probe_light_sensor;
}
