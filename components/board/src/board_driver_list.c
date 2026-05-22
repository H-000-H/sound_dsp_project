#include "driver.h"

/*
 * 显式驱动注册表 — 每增添一个新驱动, 在此添加一行
 * 替代 Linux 内核的 __initcall 链接器段方案, 更简单可控
 */

/* 外部引用各驱的注册函数 (由 DRIVER_REGISTER 宏生成) */
extern int board_driver_reg_st7789(void);
extern int board_driver_reg_max98357a(void);
extern int board_driver_reg_gpio_key(void);
extern int board_driver_reg_ws2812(void);

void board_register_all_drivers(void)
{
    board_driver_reg_st7789();
    board_driver_reg_max98357a();
    board_driver_reg_gpio_key();
    board_driver_reg_ws2812();
}
