# BSP 层说明

## 1. 宏使用规则

- 与 ESP32 芯片能力直接相关的条件编译，优先使用 ESP-IDF/SoC 自带宏。
- 典型宏包括：`SOC_WIFI_SUPPORTED`、`SOC_BT_SUPPORTED`、`SOC_BT_CLASSIC_SUPPORTED`、`SOC_RMT_SUPPORT_APB`、`CONFIG_IDF_TARGET_*`。
- 与板级连线、外部模组、产品策略相关的固定参数，统一使用项目自定义的 `CONFIG_BSP_*` 宏。

## 2. BSP 模块总开关

- 每个 BSP 模块均通过 `CONFIG_ENABLE_BSP_*` 控制整个模块的编译与类型可见性。
- 当 `CONFIG_ENABLE_BSP_xxx=0` 时：
  - 头文件内的类型声明、函数原型完全不可见。
  - 源文件内容被跳过，不产生任何符号。
  - 上层若直接包含该头文件会得到空文件；若在代码中直接调用被裁剪模块的函数，会产生链接错误，从而在移植时强制暴露依赖问题。
- 裁剪不依赖 CMake 条件编译：CMake 始终编译所有 BSP 源文件，真正裁剪由头源内的 `#if CONFIG_ENABLE_BSP_*` 完成。这样 `config.hpp` 中的宏是唯一的控制点。
- 端口到新芯片时，将不需要的 BSP 开关注销即可彻底移除对应 BSP 代码。

当前总开关宏：
| 宏 | 对应模块 |
|---|---|
| `CONFIG_ENABLE_BSP_GPIO` | GPIO 驱动 (bsp_gpio) |
| `CONFIG_ENABLE_BSP_RGB_LED` | WS2812 RGB LED (bsp_rgb_led) |
| `CONFIG_ENABLE_BSP_SPI` | SPI 驱动 (bsp_spi) |
| `CONFIG_ENABLE_BSP_I2S` | I2S 驱动 (bsp_i2s) |
| `CONFIG_ENABLE_BSP_PWM` | PWM/LEDC 驱动 (bsp_pwm) |
| `CONFIG_ENABLE_BSP_LCD_ST7789` | ST7789 屏幕 (st7789) |
| `CONFIG_ENABLE_BSP_WIFI` | Wi-Fi 驱动 (bsp_wifi) |
| `CONFIG_ENABLE_BSP_TCP` | TCP 驱动 (bsp_tcp) |
| `CONFIG_ENABLE_BSP_MQTT` | MQTT 驱动 (bsp_mqtt) |
| `CONFIG_ENABLE_BSP_BLUE` | 蓝牙 Classic (bsp_blue) |
| `CONFIG_ENABLE_BSP_RTC` | RTC 时钟 (BSP_RTC_CLOCK) |

## 3. 适用边界

- 芯片有没有 Wi-Fi、Classic Bluetooth、RMT APB 时钟源，这属于 SoC 能力，不能手写假配置。
- LCD 引脚、背光电平、RTC 时区、NTP 服务器、蓝牙默认扫描参数，这属于项目或硬件方案，应放在 `config.hpp`。

## 4. 当前已落地规则

- 所有 BSP 头源文件均被 `#if CONFIG_ENABLE_BSP_*` 包裹。
- `bsp_blue.*`：仅当 `CONFIG_ENABLE_BSP_BLUE=1` 且 SoC 支持 BT Classic 时可用；否则头文件为空、源文件不编译。
- `bsp_rgb_led.c`：RMT 时钟源优先跟随 SoC 能力宏，分辨率与默认亮度走 `CONFIG_BSP_*`。
- `BSP_RTC_CLOCK.*`：时区和 NTP 服务器从 `CONFIG_BSP_*` 读取。
- `st7789.h`：LCD 引脚与背光极性从 `CONFIG_BSP_*` 读取。
- `sys_config.h`：寄存器读写优先复用 IDF `REG_*` 宏。

## 5. 修改建议

- 想适配新芯片时，先检查 SoC 能力宏是否满足，再决定是否开启对应模块。
- 想更换板子连线或模组参数时，优先改 `components/config/config.hpp`。
- 想裁剪不需要的 BSP 模块时，将对应 `CONFIG_ENABLE_BSP_*` 设为 `0`。
