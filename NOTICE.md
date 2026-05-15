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

## 15. 未解决的问题

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
