# Sound DSP — 踩坑全记录

> 2026-05-15 主题系统 + 花屏修复 + 重构

---

## 1. 浅色/深色主题系统

**新增**: `core/inc/theme.hpp` + `core/src/theme.cpp`

- 全局 `AppTheme` 枚举（`THEME_DARK` / `THEME_LIGHT`），`theme_set()` / `theme_get()`
- 10 个颜色 getter：`th_bg` / `th_card` / `th_card_hl` / `th_text` / `th_text_sec` / `th_text_dim` / `th_border` / `th_border_dim` / `th_accent` / `th_term_bg`
- 浅色采用柔和配色（`th_bg=0xE4E5E9`, `th_card=0xF0F1F4`, `th_text=0x2C2C30`），类似 iOS 浅色风格
- 设置页添加「外观」行，toggle 切换深色/浅色

**替换硬编码颜色的文件**：
| 文件 | 替换处 |
|------|--------|
| `music_app.cpp` | ~44 处（bg, card, text, border, accent），碟片始终用 0x0E0F14 保持唱片质感 |
| `card_menu.cpp` | ~11 处 |
| `lock_screen.cpp` | ~3 处 |
| `song_list.cpp` | ~9 处 |
| `serial_app.cpp` | ~12 处（移除 CLR_BG/CLR_GRAY/CLR_WHITE/BORDER/ORANGE 等常量） |
| `status_bar.cpp` | WiFi/蓝牙/电池图标用 th_text()，浅色模式下自动变暗 |
| `settings_app.cpp` | ~10 处（已有浅色，改为读主题） |

**注意**：`th_text_sec()` 深浅主题相同（0x8E8E93），日志等级颜色（INFO/WARN/ERROR）不随主题变化，资源文件（图标/动画）不变。

---

## 2. LVGL 帧缓冲内存分配修复（花屏/卡死）

**文件**: `core/src/lvgl_main.cpp`

- 新增 `lvgl_alloc_buf()`，优先从内部 RAM（`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`）分配 LVGL 绘制缓冲区
- 仅在 `USE_LVGL_PRAM=1` 时使用 PSRAM
- 原有代码直接用 `heap_caps_malloc(byte_size, MALLOC_CAP_SPIRAM)` 优先分配 PSRAM

**根因**: ESP32-S3 的 PSRAM 通过 CPU cache 访问，WiFi `bcn_timeout` 触发重连 → Flash/NVS 操作 → cache 禁用 → PSRAM 数据损坏 → 花屏/卡死。优先使用内部 RAM 可避免此问题。

---

## 3. 串口环形缓冲区替换（自定义 ring buffer → buffer.h FIFO）

**文件**: `device_hal/serial/src/serial_console.cpp`, `device_hal/serial/inc/serial_console.hpp`

- 移除自定义手写 ring buffer（`ring_used()` / `ring_free()` / `m_ring_head` / `m_ring_tail` / `m_ring`）
- 改用 ESP-IDF 标准 `buffer.h` 中的 `FIFO_Type_Def` + `fifo_init()` / `fifo_push()` / `fifo_pop()`
- 缓冲区内存仍从 PSRAM 分配（`heap_caps_calloc(..., MALLOC_CAP_SPIRAM)`），DRAM 回退

**注意**：引入 `buffer.h` 需确认 `components/device_hal/CMakeLists.txt` 包含正确的头文件路径。CMakeLists 已添加相应 include。

---

## 4. 括号风格统一

全项目（`serial_app.cpp` / `card_menu.cpp` / `song_list.cpp` / `settings_app.hpp` / `music_app.cpp` / `serial_console.cpp` / `serial_console.hpp` / `lvgl_main.cpp` / `lock_screen.cpp` 等）

- 将所有控制语句（`if` / `for` / `while` / `switch`）的括号从同行走换行风格，统一为换行大括号

---

## 5. 其他修改

| 项目 | 详情 |
|------|------|
| 音乐播放器碟片 | 碟片背景固定在 0x0E0F14（深色唱片色），浅色模式下仍有唱片纹路 |
| 串口终端背景 | 浅色模式用 `th_term_bg()`（0xF0F1F4），深色模式用 0x08090C |
| 音量滑块 | 固定在白色（0xFFFFFF），半透明深色底色上保持一致可见 |
| 歌单长按防抖 | `m_pending_enter` 标志位防止长按 ENTER 抖动 |

---

## 6. NOTICE.html 不同步

`gen_html.py` 不在仓库中，NOTICE.md → NOTICE.html 无法自动生成。如需同步，需编写转换脚本或手动编辑 NOTICE.html。

---

# 历史架构变更（此前会话）

## 7. 凭据外移

**问题**：WiFi 和 MQTT 账号密码硬编码在 `thingscloud_app.cpp` 中。

**修改**：
- 新建 `main/Kconfig.projbuild` — 将凭据纳入 ESP-IDF `menuconfig` 管理体系
- `ThingsCloudApp::init()` 优先使用运行时设置值，为空时回退到 KConfig 默认值
- `app_freertos.cpp` 中移除所有硬编码凭据调用

---

## 8. SPI DMA 缓冲静态化

**问题**：`st7789.c` 中 `lcd_fill_rect_impl` 和 `lcd_draw_bitmap_impl` 每次刷图都 `malloc`/`free` 4KB DMA 缓冲，导致堆碎片。

**修改**：在 `bsp_lcd_init()` 中一次性分配 DMA 缓冲，后续所有操作复用同一块内存。

---

## 9. 按键模块重构 + LVGL 集成

**问题**：Button 类三个逻辑 Bug — `irq_queue` 从未创建、消抖状态未使用、`button_main()` 死循环无法与 LVGL 配合。

**修改**：
- Button 类全面重构：支持最多 8 个 GPIO，非阻塞 `process()` + 时间戳消抖
- LVGL 按键回调改用 `Button::get_pressed_gpio()`，复用完整消抖状态机
- 引脚通过 KConfig 配置

---

## 10. WiFi 事件驱动

**问题**：`ThingsCloudApp::init()` 中用 `vTaskDelay(3000)` 固定等待 WiFi 连接，浪费 3 秒且无法应对连接失败。

**修改**：改用 `EventGroup` + `IP_EVENT_STA_GOT_IP` 事件驱动，超时时间可通过 `menuconfig` 配置（默认 10 秒）。

---

## 11. BSP 层接口规范化

### void\* → typed pointer

6 个 BSP 模块的函数签名全部改为 typed pointer：`bsp_spi`、`bsp_i2s`、`bsp_pwm`、`bsp_lcd`、`bsp_blue`、`BSP_RTC_CLOCK`。

### raw_handle() 修复

`SpiMasterBus::raw_handle()` 返回类型 `void*` → `bsp_spi_handle*`，`st7789.cpp` 移除强制转换。

### Button 不再直接调 BSP

`GpioController` 新增 `install_isr_service()` / `add_isr_handler()` 静态方法，`button.cpp` 改为通过 `GpioController` 调用。

### 清除死代码

- 删除 `bsp/clock/` 和 `bsp/Digital amplifier/` 目录（空实现）
- 更新 CMakeLists.txt、README.md

### 目录名规范化

`Digital amplifier/` → `digital_amplifier/`

---

## 12. 架构边界收敛

### CMake 依赖收紧

| 组件 | 改前 | 改后 |
|------|------|------|
| `main/CMakeLists.txt` | `REQUIRES app bsp` | `REQUIRES app` |
| `service/CMakeLists.txt` | `REQUIRES bsp device_hal ...` | `REQUIRES device_hal ...`，`PRIV_REQUIRES bsp` |
| `device_hal/CMakeLists.txt` | `REQUIRES bsp driver ...` | `REQUIRES freertos config`，`PRIV_REQUIRES bsp driver` |

### HAL 公共头文件脱敏

7 个 `device_hal` 头文件不再暴露 BSP/ESP-IDF 类型（`gpio_controller.hpp`、`st7789.hpp`、`blue.hpp`、`wifi_manager.hpp`、`spi_bus.hpp`、`i2s_bus.hpp`、`pwm_controller.hpp`）。

### 基类虚析构

`ChannelDevice`、`DisplayDevice`、`LightDevice` 基类析构改为 `virtual ~...() = default`。

### BSP 错误处理补全

`bsp_mqtt.c`：`esp_mqtt_client_init` 返回值检查；`bsp_tcp.c`：`inet_pton`/`connect`/`socket` 失败时正确释放。

### 消除循环依赖

- `m_time.cpp`：移除 `#include "wifi_manager.hpp"`，改为回调注入
- `max98357A.cpp`：移除 `#include "driver/gpio.h"`，改为 `GpioController`

---

## 13. 内存优化：DIRAM .bss 从 88% 降至 31%

**问题**：DIRAM .bss 占用 206,792 字节（88.88%），主要由 LVGL 内置内存池（128 KB）和串口环形缓冲区（~65 KB）导致。

**修改**：
1. **LVGL 内存分配器切换为 C 标准库 malloc** — sdkconfig: `LV_USE_BUILTIN_MALLOC` → `LV_USE_CLIB_MALLOC`，配合 `CONFIG_SPIRAM_USE_MALLOC=y` 自动从 PSRAM 分配
2. **serial_debug_screen 缓冲区加 EXT_RAM_BSS_ATTR** — `s_stored[MAX_STORED]` 和 `s_partial[LINE_SZ]` 强制放入 `.ext_ram.bss` 段

**结果**：DIRAM .bss 206,792 → 9,432 字节，使用率 88.88% → ~31%。

---

## 14. 其他

- 删除项目根目录 `$null` 垃圾文件
- `main.hpp`：`PROGRAMMCPPTOC` → `EXTERN_C`，删除未使用的 `__ADDR2RED` 宏
- 新增 LLM API（DeepSeek）配置项 `CONFIG_LLM_API_ENABLE`

---

---

## 16. 架构重建：Linux Driver Model + DeviceTree + 组件解耦

> 2026-05-23 · 大规模架构优化

### 16.1 背景

原先的硬件描述分散在 `config/config.hpp`（311 行引脚定义）、`board/` C 结构体和 `bsp/` 多处，更换硬件需要改多处代码。引入 **JSON 格式的 mini DeviceTree** 和 **Linux Driver Model**，实现改 `board.dts.json` 即可换硬件连接。

### 16.2 核心变更

**① DeviceTree (board.dts.json)**
- 所有硬件设备统一在 `components/board/board.dts.json` 描述，包含 compatible、properties、depends_on
- 使用 ESP-IDF `EMBED_FILES` 编译进固件，不依赖文件系统
- `components/board/src/board_dts.c` 在启动时解析并构建设备链表

**② Linux Driver Model**
- `DRIVER_REGISTER` 宏让每个驱动自注册（compatible + probe + remove）
- `board_driver_probe_all()` 遍历设备、匹配 compatible、递归 probe parent 再 probe child
- `board_driver_list.c` 显式列出所有驱动注册函数，替代链接器段收集模式
- probe 成功设 `DEVICE_STATUS_PROBED`，失败设 `DEVICE_STATUS_ERROR`

**③ HAL 硬件隔离层**
- `components/hal/` 层通过函数指针表封装 ESP-IDF API
- 驱动通过 HAL 操作硬件，不直接调用 ESP-IDF（`hal_spi_bus_t`, `hal_gpio_config_t` 等）
- 新建 `hal_rmt_led.h/c` 用于 WS2812 驱动

**④ 中间件拆分**
- `middleware/` 拆分为 `system/`（服务编排）+ `service/input/`（按键输入）
- 新建 `core/` 组件：`lifecycle`, `event_bus`, `system_log`, `config_store`
- 消除 `system` ⇄ `service` 循环依赖

### 16.3 新增/更改的组件

| 组件 | 动作 | 职责 |
|------|------|------|
| `core/` | **新建** | 基类抽象：Lifecycle、EventBus、日志宏、JSON 配置读取 |
| `board/` | **增强** | DeviceTree 解析 + Driver 注册 + probe 引擎 |
| `hal/` | **新建** | ESP-IDF 隔离层（gpio/spi/i2s/pwm/uart/rmt_led） |
| `drivers/` | **新建** | 硬件驱动（st7789/max98357a/gpio_key/ws2812），自注册 |
| `system/` | **继承原 middleware** | SystemRuntime、TaskManager、服务注册表 |
| `service/` | **增强** | 增加 `input/` 子模块（KeyInput） |
| `middleware/` | **删除** | 拆分到 system/ + core/ + service/input/ |

### 16.4 目录结构（最终）

```
components/
├── core/          基类 (Lifecycle, EventBus, 日志宏, 配置)
├── board/         DeviceTree 核心 + Driver 引擎
├── hal/           ESP-IDF 隔离层 (函数指针表)
├── drivers/       硬件驱动 (自注册)
├── algorithm/     DSP 算法 (EQ)
├── system/        系统编排 (SystemRuntime, TaskManager)
├── service/       业务服务 (audio/ui/cloud/input)
├── app/           LVGL GUI
├── config/        编译时开关
```

### 16.5 依赖关系

```
app → system → service → board → drivers → hal → ESP-IDF
                ↓           ↑
              core ─────────┘
```

各层约束：

| 层级 | 不允许依赖 |
|------|-----------|
| board/ (DTS 核心) | 无上层模块 |
| hal/ | 无上层模块 |
| drivers/ | UI, app, service, lvgl |
| algorithm/ | UI, RTOS, HAL, 任何硬件 |
| service/ | 不直接调 HAL/ESP-IDF（全通过 driver） |
| core/ | 无上层模块 |
| system/ | 协调层，不直接调 hardware |

### 16.6 Lifecycle 状态机

所有 Service + SystemRuntime 继承 Lifecycle，添加状态转换守卫：

```
Created → Initialized → Started ⇄ Suspended
  ↑          ↓              ↓
  └── Stopped ←─────────────┘
  Any → Failed (不可恢复)
```

`can_transit()` 在 `core/src/lifecycle.cpp` 实现，每个生命周期方法调用前检查转换合法性，非法转换将标记 `Failed` 状态。

### 16.7 注意

- `board.dts.json` 中依赖顺序重要：parent 必须在 child 之前出现（或 `depends_on` 指向已存在的设备）
- 所有 probe 调用的 `hal_*` 函数必须能在 probe 阶段正常工作（ESP-IDF 驱动已初始化）
- 新增驱动 → `components/drivers/` 下创建 → 加 `DRIVER_REGISTER` → `board_driver_list.c` 加一行
- `EQ.h` 全局变量定义已在上一轮移入 `EQ.c`，头文件仅有 `extern` 声明（防 multiple definition）

### 16.8 编译期 DTS 升级 (2026-05-23)

从运行时 JSON 解析升级为**编译期 MCU Lite DTS**：

**变更：**
- `board.dts.json` → **`board.dts`**（MCU Lite DTS 格式，`/dts-v1/;` 头 + `&label` phandle 语法）
- `board_dts.c`（启动时 cJSON 解析） → **`board_device.c`**（使用生成的静态 `device_t` 表）
- `board.dts` 在构建时由 **`tools/dtc-lite.py`** 解析，生成 `board_nodes.h`（DEV_ID 枚举）、`board_devtable.c`（只读 `.rodata` 设备表）、`board_probe.c`（probe 函数指针表 + 拓扑排序顺序）
- `DRIVER_REGISTER` 宏产生的函数由 dtc-lite.py 编译期扫描收录，**无运行时 strcmp 匹配**
- `board_driver_probe_all()` 改按拓扑排序顺序调用，不再递归 probe parent
- `board_driver_list.c` 弃用（被生成的 probe 表取代）
- **删除 cJSON 依赖**，`CMakeLists.txt` 不再 `REQUIRES json`

**关键点：**
- `tools/dtc-lite.py` 是纯 Python tokenizer + 递归下降解析器，通过 `find_package(Python3)` 在构建时执行
- 生成的头文件输出到 `${CMAKE_CURRENT_BINARY_DIR}/generated/`，通过 `target_sources()` 加入编译
- soc 级别设备（esp32,spi-bus / esp32,i2s-bus / esp32,uart）在验证时跳过，无需注册驱动
- 新增硬件 → 改 `board.dts`（添加设备节点 + compatible）→ 创建驱动 .c 文件（加 `DRIVER_REGISTER`）→ 构建

---

---

## 17. 架构重建第二阶段：Media层 + Capability边界 + System单一Runtime

> 2026-05-23 · 解决 5 个隐性耦合风险点

### 17.1 变更概要

| 风险点 | 问题 | 修改 |
|--------|------|------|
| ① MP3 放在 algorithm/ | 纯算法组件混入解码器 + buffer + I2S sink | 新建 `media/` 层，MP3 移入 |
| ② core/ 垃圾桶 | EventBus + config_store + 杂项混杂 | config_store 移入 `config/`，core 仅保留 event_bus |
| ③ service/driver 边界模糊 | AudioService 可直接调 I2S/buffer | 新建 `capability/` 层（led_engine, input_engine），service 只调 capability |
| ④ system 非唯一 runtime 所有者 | CloudService 直接调 esp_netif_init/esp_event_loop_create_default | 移入 `SystemRuntime::init()` |
| ⑤ app 接触 FreeRTOS | thingscloud_app 直接调 xTaskCreatePinnedToCore | 改用 `board_task_create()`（board/ 组件提供） |

### 17.2 新增组件

**media/** — 媒体管线层
- `media/mp3.hpp` — MP3 解码器封装，含 I2S 输出、EQ 滤波、音量控制
- 位于 algorithm/（纯算法）和 capability/（硬能力入口）之间
- `REQUIRES board hw_hal core algorithm chmorgan__esp-libhelix-mp3`

**capability/** — 硬能力入口层
- `led_engine.hpp/c` — C 函数指针表封装 WS2812，service 通过 `device_find("rgb_led0")` 获取设备
- `input_engine.hpp/c` — C 函数指针表封装 gpio_key，service 通过 `device_find("buttons0")` 获取设备
- 约束：service 不直接调 driver/HAL，全通过 capability

### 17.3 关键设计决策

**board_task_create()** 置于 board/ 组件：
- system/TaskManager 依赖 service，service 无法反向依赖 system
- board/ 是最底层组件，board_task_create() 作为 C 封装绕开循环依赖
- system/TaskManager 的 create_task() 也基于它实现

**config_store** 独立为 config/ 组件：
- `EMBED_TXTFILES` 路径相对于组件目录：`../../assets/config/system_config.json`
- `INCLUDE_DIRS "include" "."` 同时暴露新式 include/ 和根目录 config.hpp

### 17.4 依赖约束（最终）

```
app → system → service → capability → board → drivers → hal → ESP-IDF
                ↓                                        ↑
              media ──────────────────────────────────────┤
              algorithm (纯 DSP, 无硬件依赖)               │
              config (EMBED_TXTFILES, 无框架依赖) ────────┘
```

| 层级 | 不允许依赖 |
|------|-----------|
| media/ | service, app, ui |
| capability/ | service, app, ui |
| service/ | driver, HAL, ESP-IDF（全通过 capability） |
| system/ | app, ui（可调 service 生命周期） |

---

## 18. 未解决的问题

### PC 关闭串口时系统自动重启

**现象**：PC 端关闭串口终端（如关闭 PuTTY/minicom）后，ESP32-S3 立即重启。

**根因分析**：
- ESP32-S3 的 USB Serial/JTAG 外设在 host 断连后，USB PHY 可能进入挂起状态，此时访问 TX FIFO 寄存器（`usb_serial_jtag_ll_write_txfifo` / `txfifo_flush`）会导致总线挂起 → CPU 无法响应 → WDT → 重启
- 没有可靠的主机在位寄存器可供读取来判断连接状态
- `usb_serial_jtag_ll_phy_is_pad_enabled()` 读取的是静态配置位 `usb_pad_enable`，不随连接状态变化
- `usb_serial_jtag_ll_txfifo_writable()` 读取 `serial_in_ep_data_free`（TX FIFO 是否有空间），host 断开后 FIFO 不再被消费，塞满后不可写——但仅在主动写入时才能检出

**已尝试的解决方案**：
1. 中断屏蔽 — `usb_serial_jtag_ll_disable_intr_mask()` 禁用所有 USB 中断，避免 `BUS_RESET` 等中断无 ISR 处理导致 panic
2. 链式调用原始 vprintf — 让 ESP-IDF 框架自身处理 USB 输出，不直接访问寄存器（未解决）
3. `write()` 写入前检查 `txfifo_writable()`，满时丢弃数据，标记 `m_connected = false`

**当前状态**：未能彻底解决。写入 USB TX FIFO 时若 host 已断连，仍可能触发硬件挂起。

---

## 19. ⚠️ DRIVER_REGISTER 链接器踩坑：纯 probe 表驱动的符号被吞

> 2026-05-25 · light_sensor 驱动链接失败，修正前分析有误，此为最终正确根因

### 19.1 现象

`DRIVER_REGISTER(light_sensor, ...)` 生成的 `board_driver_probe_light_sensor` 链接时报 `undefined reference`，而前 4 个驱动（st7789/max98357a/gpio_key/ws2812）用同样的 `DRIVER_REGISTER` 却能链过。

### 19.2 真正的根因：显式引用 vs. 仅有 probe 表引用

从 map 文件可以清楚看到前 4 个被拉入的原因：

```
st7789_driver.c.obj    ← app/libapp.a(lvgl_main.cpp.obj)      调了 st7789_init()
max98357a_driver.c.obj ← capability/audio_engine.cpp.obj      调了 max98357a_init()
gpio_key_driver.c.obj  ← capability/input_engine.cpp.obj      调了 gpio_key_init()
ws2812_driver.c.obj    ← capability/led_engine.cpp.obj        调了 ws2812_init()
```

前 4 个驱动有 **两套引用**：
1. 旧的直接函数调用（`st7789_init()` 等）← 这是它们被从 `libdrivers.a` 拉入的原因
2. probe 表的函数指针 ← 这是架构设计的运行时路径

light_sensor 只有 **一套引用**：probe 表的函数指针。

**GNU ld 归档提取规则**：扫到 `libdrivers.a` 时，只拉入定义了"当前未定义符号"的 `.o`。此时 `board_probe.c.obj` 还没被拉进来（因为 app_main 还没被处理），probe 表里的 5 个函数指针引用都还没产生。所以 light_sensor 只靠 probe 表是**无法被拉入的**。

前 4 个则是运气好——它们被其他代码路径的调用**顺带拉进来了**。

### 19.3 解法：-u 显式标记

```cmake
target_link_options(${COMPONENT_LIB} INTERFACE
    "-u" "board_driver_probe_light_sensor"
)
```

`-u` 在**所有归档被扫描之前**就把符号标记为 needed，等链接器扫到 `libdrivers.a` 时 light_sensor 就会被正确拉入。

**前 4 个不需要加**，因为它们的旧代码调用路径充当了隐式的 `-u`。但**架构上它们和 light_sensor 是平等的**——如果以后重构去掉旧调用路径（所有驱动只通过 probe 表激活），那么前 4 个也会掉进同样的坑。

### 19.4 新增驱动的两种做法

| 方案 | 做法 | 适用场景 |
|------|------|---------|
| **A. 加 `-u`** | `board/CMakeLists.txt` 加一行 `"-u" "board_driver_probe_xxx"` | 纯 probe 表激活，无其他代码调用 |
| **B. 直接使用** | 在 capability/service 层调一次 `xxx_init(dev)` / `xxx_read()` | 驱动集成到功能里时自然解决 |

两种都是正确做法，方案 A 更适合"只靠 probe 表活的驱动"。

### 19.5 为什么 board_force_link.c 也不行

`board_force_link.c` 所有符号都是 `static`（通过 `objdump -t` 确认全是 `l` 标记）。链接器检查 `.a` 里的 `.o` 时只看**全局符号表**，没有全局符号的 `.o` 会被**跳过**，构造函数永远不会被执行。`.init_array` 的 `KEEP` 只对已提取的目标文件生效，对归档提取阶段无效。

### 19.6 其他尝试的误区

- **`target_link_options(PRIVATE ...)`**：之前认为 PRIVATE 不传播到最终链接，实际 ESP-IDF 里组件静态库的 PRIVATE 链接选项也会出现在最终 elf 的 LINK_FLAGS 中。所以 PRIVATE/INTERFACE 不是根因。
- **`"-u board_driver_probe_light_sensor"` 合在一个引号里**：CMake 会将整个带空格的字符串作为单个参数传给链接器。链接器无法正确解析 `" -u board_driver_probe_light_sensor"` 为一个参数，导致 `-u` 不生效。必须分两个参数 `"-u" "board_driver_probe_light_sensor"`。
- 两个错误叠加导致 `-u` 实际没传进链接器，所以链接失败。

---

## 20. App 层状态机引入

### 20.1 动机

音乐播放器涉及歌曲切换、暂停/播放、进度跟踪等异步动作，之前依赖 `m_playing` 布尔值 + `m_adjust` 标志位做状态判断，容易导致以下问题：

- 歌曲切换中（文件 I/O）再次收到按键，状态错乱
- Loading 未完成就响应播放/暂停指令
- 无明确的错误恢复路径

### 20.2 改了什么

**MusicApp** (`components/app/lvgl/UI/app/`)

新增 `MusicState` 枚举，5 个状态：

| 状态 | 含义 | 进入条件 |
|---|---|---|
| `kIdle` | 初始/停止，无歌曲加载 | show() / ESC 退出 |
| `kLoading` | 文件 I/O 或解码准备中 | on_song_select / on_next / on_prev |
| `kPlaying` | 正常播放 | Loading 完成 / Paused → 恢复 |
| `kPaused` | 暂停 | Playing → 暂停 |
| `kError` | 文件缺失或解码失败 | (预留) |

`set_state()` 包含非法转移检测（`ESP_LOGW` + 拒绝），`handle_key()` 在 `kLoading`/`kError` 时只放行 ESC，其余按键被守卫；`progress_timer_cb` 仅在 `kPlaying` 时推进进度。

**CardMenu** (`components/app/lvgl/UI/nav/`)

新增 `MenuState` 枚举，替换原来的 `s_animating` bool：

| 状态 | 含义 |
|---|---|
| `kIdle` | 菜单正常显示，可交互 |
| `kAnimating` | 卡片滑动动画中 |
| `kAppActive` | 子 App 运行中（屏蔽 GPIO 轮询） |

### 20.3 设计原则

- 不改变现有行为：状态机只做守卫（guard），不做行为变更
- 同步兼容：MusicImpl 若不同步调用 `set_state()`，状态机在钩子返回后自动 fallback 到 `kPlaying`
- 日志可见：所有状态转移都打 `ESP_LOGI`，非法转移打 `ESP_LOGW`
- 序列化和 settings_app 的 UI 层未引入状态机，因其操作是纯同步的，状态机无实际收益
