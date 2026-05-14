# 架构修复与优化记录

## 1. 安全：凭据外移

**问题**：WiFi 和 MQTT 账号密码硬编码在 `thingscloud_app.cpp` 中。

**修改**：
- 新建 [main/Kconfig.projbuild](main/Kconfig.projbuild) — 将凭据纳入 ESP-IDF `menuconfig` 管理体系
- `ThingsCloudApp::init()` 优先使用运行时设置值，为空时回退到 KConfig 默认值
- `app_freertos.cpp` 中移除所有硬编码凭据调用

**涉及文件**：
- `main/Kconfig.projbuild` — 新增
- `components/app/src/thingscloud_app.cpp` — 重构 init()
- `components/app/src/app_freertos.cpp` — 移除硬编码参数

---

## 2. 性能：SPI DMA 缓冲静态化

**问题**：`st7789.c` 中 `lcd_fill_rect_impl` 和 `lcd_draw_bitmap_impl` 每次刷图都 `malloc`/`free` 4KB DMA 缓冲，导致堆碎片。

**修改**：在 `bsp_lcd_init()` 中**一次性分配** DMA 缓冲，后续所有操作复用同一块内存。

**涉及文件**：
- `components/bsp/lcd/src/st7789.c`

---

## 3. 按键模块 Bug 修复 + LVGL 集成

**问题**：Button 类有三个逻辑 Bug：
- `irq_queue` 从未创建（所有中断事件静默丢弃）
- `DebouncePress`/`DebounceRelease` 状态定义了但从未使用
- `button_main()` 是死循环，无法与 LVGL 轮询配合

**修改**：
- Button 类全面重构：支持最多 8 个 GPIO（原为单 GPIO 单例）
- 非阻塞 `process()` + 时间戳消抖，取代原来的阻塞 `vTaskDelay`
- LVGL 按键回调改用 `Button::get_pressed_gpio()`，复用完整的消抖状态机
- 新增 KConfig 选项配置按键 GPIO 引脚

**涉及文件**：
- `components/device_hal/button/inc/button.hpp` — 重构
- `components/device_hal/button/src/button.cpp` — 重构
- `components/app/lvgl/src/lvgl_main.cpp` — 集成 Button 状态机
- `components/config/config.hpp` — 按键引脚默认值
- `main/Kconfig.projbuild` — 按键引脚 menuconfig 选项

---

## 4. 网络：WiFi 等待改为事件驱动

**问题**：`ThingsCloudApp::init()` 中用 `vTaskDelay(3000)` 固定等待 WiFi 连接，浪费 3 秒且无法应对连接失败。

**修改**：改用 `EventGroup` + `IP_EVENT_STA_GOT_IP` 事件驱动，WiFi 连接成功立即继续，超时时间可通过 `menuconfig` 配置（默认 10 秒）。

**涉及文件**：
- `components/app/src/thingscloud_app.cpp`

---

## 5. 代码清理

**修改**：
- 删除项目根目录的 `$null` 垃圾文件
- `main.hpp`：`PROGRAMMCPPTOC` → `EXTERN_C`（语义清晰），删除未使用的 `__ADDR2RED` 宏
- `main.cpp`：同步更新宏引用

**涉及文件**：
- `main/main.hpp`
- `main/main.cpp`

---

## 6. LLM API 配置

**新增**：DeepSeek（OpenAI 兼容协议）API 配置项，为未来设备端 AI 调用做准备。

| 配置 | 默认值 |
|------|--------|
| `CONFIG_LLM_API_ENABLE` | n |
| `CONFIG_LLM_API_BASE_URL` | `https://api.deepseek.com/v1` |
| `CONFIG_LLM_API_MODEL` | `deepseek-chat` |
| `CONFIG_LLM_API_TIMEOUT_MS` | 30000 |

**涉及文件**：
- `main/Kconfig.projbuild` — 新增 menu
- `components/config/config.hpp` — 新增默认值

---

---

## 8. 架构边界收敛：HAL 抽象化 + 依赖收紧

### 8.1 CMake 依赖收紧

| 组件 | 修改前 | 修改后 |
|------|--------|--------|
| `main/CMakeLists.txt` | `REQUIRES app bsp` | `REQUIRES app` |
| `service/CMakeLists.txt` | `REQUIRES bsp device_hal ...` | `REQUIRES device_hal ...`，`PRIV_REQUIRES bsp` |
| `device_hal/CMakeLists.txt` | `REQUIRES bsp driver ...` | `REQUIRES freertos config`，`PRIV_REQUIRES bsp driver` |

**涉及文件**：
- `main/CMakeLists.txt`
- `components/service/CMakeLists.txt`
- `components/device_hal/CMakeLists.txt`

### 8.2 HAL 公共头文件脱敏

7 个 `device_hal` 头文件不再暴露 BSP/ESP-IDF 类型：

| 头文件 | 抽象方式 |
|--------|----------|
| `gpio_controller.hpp` | 自定义 `GpioMode`/`GpioPull`/`GpioInterrupt` 枚举，替代 `gpio_mode_t` 等 |
| `st7789.hpp` | PImpl 隐藏 `bsp_spi_handle*`/`bsp_lcd_handle_t` |
| `blue.hpp` | PImpl 隐藏 `bsp_blue_handle_t`/`esp_a2d_cb_event_t` |
| `wifi_manager.hpp` | 自定义 `WifiMode::Sta/Ap/ApSta`，替代 `wifi_mode_t` |
| `spi_bus.hpp` | `void* native_handle()` 替代暴露 `bsp_spi_handle` |
| `i2s_bus.hpp` | 纯业务接口，无 BSP 类型 |
| `pwm_controller.hpp` | 纯业务接口，无 BSP 类型 |

**涉及文件**：
- `components/device_hal/gpio/inc/gpio_controller.hpp`
- `components/device_hal/lcd/inc/st7789.hpp`
- `components/device_hal/blue/inc/blue.hpp`
- `components/device_hal/network/inc/wifi_manager.hpp`
- `components/device_hal/spi/inc/spi_bus.hpp`
- `components/device_hal/i2s/inc/i2s_bus.hpp`
- `components/device_hal/pwm/inc/pwm_controller.hpp`

### 8.3 基类虚析构补全

`ChannelDevice`、`DisplayDevice`、`LightDevice` 基类析构改为 `virtual ~...() = default`。

**涉及文件**：
- `components/device_hal/base/inc/channel_device.hpp`
- `components/device_hal/base/inc/display_device.hpp`
- `components/device_hal/base/inc/light_device.hpp`

### 8.4 BSP 错误处理补全

- `bsp_mqtt.c`：`esp_mqtt_client_init` 返回值检查，register event 失败时 destroy client
- `bsp_tcp.c`：`inet_pton`/`connect`/`socket` 失败时正确释放资源并返回 false

**涉及文件**：
- `components/bsp/mqtt/src/bsp_mqtt.c`
- `components/bsp/tcp/src/bsp_tcp.c`

### 8.5 消除 device_hal 模块间循环依赖

- `m_time.cpp`：移除 `#include "wifi_manager.hpp"`，改为回调注入 `Time::set_net_available_checker()`
- `max98357A.cpp`：移除 `#include "driver/gpio.h"`，改为通过 `GpioController` 控制 SD 引脚

**涉及文件**：
- `components/device_hal/time/inc/m_time.hpp`
- `components/device_hal/time/src/m_time.cpp`
- `components/device_hal/digital_amplifier/src/max98357A.cpp`

## 配置入口

所有新增配置项均可在 `idf.py menuconfig` 中修改：
```
ThingsCloud Configuration       → WiFi/MQTT 凭据
LVGL Key GPIO Configuration     → 物理按键引脚
LLM API Configuration           → DeepSeek 等 API 设置
```

---

## 7. 架构修复：BSP 层接口规范化

### 7.1 BSP `void*` 改为 typed pointer

**问题**：BSP 公共 API 全部使用 `void* param` 传递句柄，丢失编译期类型检查，类型错误只能在运行时 crash 时暴露。

**修改**：6 个 BSP 模块的函数签名全部改为 typed pointer：

| 模块 | 改动 |
|------|------|
| `bsp_spi` | `void* param` → `bsp_spi_handle* param` |
| `bsp_i2s` | `void* param` → `bsp_i2s_handle* param` |
| `bsp_pwm` | `void* param` → `bsp_pwm_handle_t* param` / `bsp_timer_handle_t* param` |
| `bsp_lcd` | `void* param, void* arg` → `bsp_spi_handle* param, bsp_lcd_handle_t* arg` |
| `bsp_blue` | `void* param` → `bsp_blue_handle_t* param` |
| `BSP_RTC_CLOCK` | `void* param` → `BSP_RTC_Clock_HANDLE_t* param` |

**涉及文件**：
- `components/bsp/spi/inc/bsp_spi.h` — 声明
- `components/bsp/spi/src/bsp_spi.c` — 实现及本地变量转换
- `components/bsp/i2s/inc/bsp_i2s.h` — 声明
- `components/bsp/i2s/src/bsp_i2s.c` — 实现
- `components/bsp/pwm/inc/bsp_pwm.h` — 声明
- `components/bsp/pwm/src/bsp_pwm.c` — 实现
- `components/bsp/lcd/inc/st7789.h` — 声明
- `components/bsp/lcd/src/st7789.c` — 实现及所有 static 辅助函数
- `components/bsp/blue/inc/bsp_blue.h` — 声明
- `components/bsp/blue/src/bsp_blue.c` — 实现
- `components/bsp/rtc/inc/BSP_RTC_CLOCK.h` — 声明
- `components/bsp/rtc/src/BSP_RTC_CLOCK.c` — 实现

### 7.2 修复 `raw_handle()` 破坏封装

**问题**：`SpiMasterBus::raw_handle()` 返回 `void*`，调用方必须 `static_cast`，BSP 内存布局泄漏到 Device HAL。

**修改**：返回类型 `void*` → `bsp_spi_handle*`，`st7789.cpp` 移除强制转换。

**涉及文件**：
- `components/device_hal/spi/inc/spi_bus.hpp`
- `components/device_hal/spi/src/spi_bus.cpp`
- `components/device_hal/lcd/src/st7789.cpp`

### 7.3 Button 不再直接调 BSP

**问题**：`button.cpp` 直接调用 `gpio_driver_install_isr_service()` / `gpio_driver_add_isr_handler()`，绕过 Device HAL 层。

**修改**：`GpioController` 新增 `install_isr_service()` / `add_isr_handler()` 静态方法，`button.cpp` 改为通过 `GpioController` 调用。

**涉及文件**：
- `components/device_hal/gpio/inc/gpio_controller.hpp`
- `components/device_hal/gpio/src/gpio_controller.cpp`
- `components/device_hal/button/src/button.cpp`

### 7.4 清理 BSP 跨模块依赖

**问题**：`bsp_spi.h` 包含了 `bsp_gpio.h` 和 `sys_config.h`（均未使用），`bsp_tcp.h` 包含了 `bsp_wifi.h`（未使用）。

**修改**：删除所有不必要的 `#include`。

**涉及文件**：
- `components/bsp/spi/inc/bsp_spi.h`
- `components/bsp/tcp/inc/bsp_tcp.h`
- `components/bsp/rtc/inc/BSP_RTC_CLOCK.h`

### 7.5 删除死代码

**问题**：`bsp/clock/esp32_clock.c` 所有函数为空实现；`bsp/Digital\ amplifier/` 头文件和源文件均为空；`main.cpp` 调用了空的 `init()`。

**修改**：
- 删除 `bsp/clock/` 目录
- 删除 `bsp/Digital\ amplifier/` 目录
- 更新 `bsp/CMakeLists.txt` 移除已删文件
- `main.hpp` 移除 `#include "esp32_clock.h"`
- `main.cpp` 移除空 `init()` 调用
- `max98357A.hpp` 移除 `#include "bsp_max98357A.h"`
- 更新 `bsp/README.md` 和 `config/README.md` 模块列表

**涉及文件**：
- `components/bsp/clock/` — 删除
- `components/bsp/Digital amplifier/` — 删除
- `components/bsp/CMakeLists.txt` — 更新
- `components/bsp/README.md` — 更新
- `components/config/README.md` — 更新
- `main/main.hpp` — 更新
- `main/main.cpp` — 更新
- `components/device_hal/digital_amplifier/inc/max98357A.hpp` — 更新

### 7.6 目录名规范化

**问题**：`device_hal/Digital amplifier/` 含空格，跨平台工具链存在隐患，且与其他目录命名风格不一致。

**修改**：`Digital amplifier/` → `digital_amplifier/`

**涉及文件**：
- `components/device_hal/digital_amplifier/` — 重命名

## 8. 内存优化：DIRAM .bss 从 88% 降至 31%

**问题**：DIRAM .bss 占用 206,792 字节（88.88%），主要由两个大静态数组导致。

| 占用者 | 大小 | 原因 |
|--------|------|------|
| LVGL 内置内存池 work_mem_int | **128 KB** | lv_mem_core_builtin.c 静态数组 |
| serial_debug_screen 环形缓冲区 s_stored[250] | **≈65 KB** | StoredLine 每条 262 字节 × 250 条 |

**修改**：

1. **LVGL 内存分配器切换为 C 标准库 malloc**
   - sdkconfig: LV_USE_BUILTIN_MALLOC → LV_USE_CLIB_MALLOC
   - LVGL 改用 malloc/ree，配合 CONFIG_SPIRAM_USE_MALLOC=y 自动从 PSRAM 分配

2. **serial_debug_screen 缓冲区加 EXT_RAM_BSS_ATTR**
   - components/app/lvgl/UI/src/serial_debug_screen.cpp
   - s_stored[MAX_STORED] → EXT_RAM_BSS_ATTR
   - s_partial[LINE_SZ] → EXT_RAM_BSS_ATTR
   - 强制放入 .ext_ram.bss 段（PSRAM）

**结果对比**：

| 指标 | 改前 | 改后 |
|------|------|------|
| DIRAM .bss | 206,792 bytes | 9,432 bytes |
| DIRAM 使用率 | **88.88%** | **~31%** |
| PSRAM .ext_ram.bss | 14,024 bytes | 80,280 bytes |

**涉及文件**：
- sdkconfig — LV_USE_BUILTIN_MALLOC → LV_USE_CLIB_MALLOC
- components/app/lvgl/UI/src/serial_debug_screen.cpp — 添加 EXT_RAM_BSS_ATTR
- managed_components/lvgl__lvgl/Kconfig — 引用了 CONFIG_LV_USE_CLIB_MALLOC 选项
