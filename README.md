# Sound DSP Project (ESP32-S3)

> **踩坑记录** → [`NOTICE.html`](./NOTICE.html)（13 个已解决的坑，含解决方案）

---

## 目录
1. [架构总览](#1-架构总览)
2. [为什么选 ESP32-S3](#2-为什么选-esp32-s3)
3. [内存布局](#3-内存布局)
4. [导航流](#4-导航流)
5. [按键映射](#5-按键映射)
6. [模块职责](#6-模块职责)
7. [工厂模式](#7-工厂模式)
8. [开发规范](#8-开发规范)
9. [DSP / EQ（待实现）](#9-dsp--eq待实现)

---

## 1. 架构总览

```
┌───────────────────────────────────────────────────────────────────┐
│  app 层（LVGL GUI + ThingsCloud 编排）                              │
│  ┌─────────────┐  ┌──────┐ ┌───────┐ ┌─────────────────────────┐ │
│  │ ThingsCloud  │  │ core │ │screen │ │       nav/card          │ │
│  │ WiFi/MQTT    │  │ 主循环 │ │ 锁屏   │ │       卡片轮播          │ │
│  ├─────────────┤  ├──────┤ ├───────┤ ├────────────┬────────────┤ │
│  │ FreeRTOS    │  │ app  │ │ res   │ │  Settings  │   Music    │ │
│  │ lcd_task C0 │  │ 设置  │ │图片/字│ │  WiFi/MQTT │  播放器+选歌 │ │
│  │ mqtt_task C1│  │ 音乐  │ │库/Lottie│ │  亮度/低功耗│  音量同步   │ │
│  │             │  │ 串口  │ │       │ │            │            │ │
│  └─────────────┘  └──────┘ └───────┘ └────────────┴────────────┘ │
├───────────────────────────────────────────────────────────────────┤
│  device_hal 层 — C++ 封装，对上层隐藏 ESP-IDF 细节                   │
│  GPIO · LCD(ST7789) · I2S · PWM · Button · Bluetooth · RTC       │
│  SPI · RGB LED · SerialConsole · Network(WiFi) · DigitalAMP       │
├───────────────────────────────────────────────────────────────────┤
│  bsp 层 — 纯 C，直接调 ESP-IDF driver                                 │
│  GPIO LCD SPI I2S PWM WiFi TCP MQTT BT RTC                        │
├───────────────────────────────────────────────────────────────────┤
│  algorithm 层 — 纯计算，零硬件依赖                                     │
│  buffer/FIFO · DSP/EQ（待实现）                                      │
├───────────────────────────────────────────────────────────────────┤
│  ESP-IDF — FreeRTOS · LWIP · ESP-NETIF · MBEDTLS · DRIVERS        │
└───────────────────────────────────────────────────────────────────┘
```

## 2. 为什么选 ESP32-S3

| 需求 | C3 | C6 | S3 |
|------|----|----|-----|
| PSRAM（本项目刚需） | ❌ 不支持 | ❌ 不支持 | ✅ Octal SPI 8MB |
| Lottie 渲染缓冲区 160KB | ❌ 内部 SRAM 仅 400KB | ❌ 内部 SRAM 仅 512KB | ✅ PSRAM 分配 |
| 240×240 圆形屏 + LVGL 9.x | ⚠️ 勉强（单核） | ⚠️ 勉强（单核） | ✅ 双核 LX7 |
| 音频解码 + EQ 滤波实时性 | ❌ 单核无法同时跑 UI+音频 | ❌ 同左 | ✅ UI 核 + 音频核分离 |
| Octal SPI 屏幕刷新 | ❌ 仅 Quad SPI | ❌ 仅 Quad SPI | ✅ Octal SPI |
| AI 加速器 | ❌ | ❌ | ✅ 向量指令集 |
| USB-OTG | ❌ | ❌ | ✅ |

> **S3 不可替代**：Lottie 渲染需要 PSRAM，C3/C6 均无 PSRAM 控制器。双核架构允许 LVGL 渲染 (Core 0) 和音频解码 (Core 1) 分离，单核 RISC-V 做不到。

## 3. 内存布局

```
┌─────────────────────────────────────┐
│  内部 SRAM ~512KB                    │
│  ├─ FreeRTOS 内核 + 任务栈    ~80KB  │
│  ├─ LVGL draw buffer + TLSF ~140KB  │
│  ├─ lwIP 网络缓冲            ~60KB  │
│  ├─ WiFi 栈                  ~80KB  │
│  └─ 其他                     剩余   │
├─────────────────────────────────────┤
│  PSRAM 8MB                          │
│  ├─ LVGL 显示缓冲            ~192KB │
│  ├─ Lottie 渲染缓冲           160KB │
│  ├─ LCD 任务栈                 8KB  │
│  ├─ 串口环形缓冲              ~64KB │
│  └─ 剩余给算法/音频           ~7.5MB │ ← DSP EQ 待用
└─────────────────────────────────────┘
```

| 优化项 | 优化前 | 优化后 | 收益 |
|--------|--------|--------|------|
| Lottie 缓冲区 | `.bss` 段（内部 SRAM） | PSRAM 动态堆 | 释放 160KB SRAM |
| 显示缓冲区 | `MALLOC_CAP_DMA` (DRAM) | `MALLOC_CAP_SPIRAM` | 56KB 移出 DRAM |
| `LV_MEM_SIZE` | 32KB | 128KB | 解决 draw layer 死锁 |
| `.dram0.bss` | 9,344 B | 5,376 B | ↓42% |
| 总静态 DRAM | ~31.3 KB | ~26.5 KB | ↓15% |

## 4. 导航流

```
🔒 LockScreen ─▶ 📋 CardMenu ─▶ ⚙️ 设置 / 🎵 音乐 / 🔌 串口
       ▲                              │
       └──────────── ESC ──────────────┘
```

- **Layer 1**：锁屏 — 全屏紫灰色 + 状态栏（WiFi/蓝牙/电量/时间），ENTER → Layer 2
- **Layer 2**：3 卡横向轮播（设置/音乐/串口），ENTER → Layer 3，ESC → Layer 1
- **Layer 3**：功能页 — 设置（WiFi/蓝牙/MQTT/亮度）、音乐（播放器+选歌）、串口（终端/HEX+日志）

## 5. 按键映射

| 按键 | GPIO | 功能 |
|------|------|------|
| PREV | 17 | 焦点上移 / 减小值 / 左滑 |
| NEXT | 16 | 焦点下移 / 增大值 / 右滑 |
| ENTER | 3 | 解锁 / 确认 / 切换 |
| ESC | 46 | 返回上级 |

> **⚠ 已知问题**：LVGL 9.5 的 `LV_INDEV_TYPE_KEYPAD` 在对象加入 group 后，PREV/NEXT 被 group 内部焦点管理吞噬，不发送 `LV_EVENT_KEY`。当前通过 GPIO 轮询绕过（详见 `NOTICE.html`）。

检测流程：GPIO 中断 → ISR 入队 FreeRTOS 队列 → 主循环 `Button::process()` 出队 → `get_pressed_gpio()` 读取

### ST7789 7 线无 CS 注意事项

- **SPI Mode 3 必须**（`interface_cfg.mode = 3`）：CPOL=1, CPHA=1，否则屏幕不显示
- **无 CS 引脚**（`spics_io_num = -1`）：DC 引脚代替命令/数据区分
- **半双工**（`flags = SPI_DEVICE_HALFDUPLEX`）：7 线模组没有 MISO，只能发送不能接收
- 配置见 <span class="file">components/device_hal/spi/src/spi_bus.cpp</span>

## 6. 模块职责

| 模块 | 解决什么问题 | 入→出 |
|------|-------------|-------|
| **lvgl/** | 240×240 屏幕 GUI：锁屏、菜单、设置、音乐播放器、串口终端 | 按键事件 → 屏幕绘制 |
| **core/** | LVGL 初始化、GPIO→LV_KEY 映射、异步任务调度 | 硬件初始化 → 主循环 |
| **screen/** | 锁屏界面 + 状态栏 + 齿轮 Lottie 动画 | 系统状态 → 顶部栏 |
| **nav/** | 3 卡轮播菜单，AppBase* 管理应用跳转 | ENTER → 指定 App |
| **app/settings** | WiFi/蓝牙/MQTT/低功耗 开关 + 亮度调节 | 用户操作 → 硬件状态 |
| **app/music** | WAV 播放、进度控制、选歌列表、播放模式 | 用户操作 → I2S 输出 |
| **app/serial** | USB-JTAG 串口数据显示、日志捕获、HEX/终端 | UART 数据 → 终端渲染 |
| **ThingsCloudApp** | WiFi 连接/MQTT 协议/数据上报全生命周期 | 云指令 → RGB LED |
| **device_hal/** | C++ 设备抽象，隐藏 ESP-IDF 细节 | BSP 句柄 → 业务接口 |
| **bsp/** | 硬件初始化，直接调 ESP-IDF driver | KConfig → 硬件就绪 |
| **factory/** | 统一设备获取入口，解耦业务层和硬件层 | "我要 WiFi" → WifiManager* |
| **algorithm/dsp** | 多段 EQ 滤波（待实现），纯算法不依赖硬件 | 音频流 → 滤波后音频流 |

## 7. 工厂模式

```cpp
auto* wifi  = factory_config::network::get_wifi();
auto* mqtt  = factory_config::network::get_mqtt();
auto* lcd   = factory_config::screen::get_device();
auto* led   = factory_config::led::get_device();
auto* time  = factory_config::time::get_time();
auto* btn   = factory_config::button::get_button();
auto* blue  = factory_config::blue::get_blue();
```

## 8. 开发规范

- **不擅自改代码**：有修改建议先说明理由再动手
- **新架构先问**：模块级变动必须先讨论
- **不构建多余抽象**：三个相似行比过早封装好
- **每次改完写 NOTICE**：关键踩坑点写入 `NOTICE.html`
- **文件标注日期**：所有 .html/.md 标注 `YYYY-MM-DD`
- **层级依赖**：`app` 不直接引用 BSP，`algorithm` 零硬件依赖
- **C++ 规范**：禁用 RTTI/异常，跨文件回调用静态成员函数

## 9. DSP / EQ（待实现）

多段均衡器，Biquad 级联：

```
        b₀ + b₁·z⁻¹ + b₂·z⁻²
H(z) = ──────────────────────
        a₀ + a₁·z⁻¹ + a₂·z⁻²
```

- 可配置 N 段 EQ（典型 5 段）
- Q15/Q31 定点运算
- 先在 `py/dsp.ipynb` 验证 → 同步到 `dsp.c`

---

> 完整开发时间线、12 个关键坑及解决方案、编译错误修复、系统配置参考 → [`NOTICE.html`](./NOTICE.html)