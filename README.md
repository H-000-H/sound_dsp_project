# Sound DSP Project (ESP32-S3)

> **踩坑记录** → [`NOTICE.html`](./NOTICE.html)
> **架构变更**：2026-05-23 完成 Linux Driver Model + DeviceTree 架构重建

---

## 目录
1. [架构总览](#1-架构总览)
2. [为什么选 ESP32-S3](#2-为什么选-esp32-s3)
3. [内存布局](#3-内存布局)
4. [导航流](#4-导航流)
5. [按键映射](#5-按键映射)
6. [模块职责](#6-模块职责)
7. [DeviceTree 硬件描述](#7-devicetree-硬件描述)
8. [开发规范](#8-开发规范)
9. [DSP / EQ](#9-dsp--eq)

---

## 1. 架构总览

```
┌───────────────────────────────────────────────────────────────────┐
│  app 层 (LVGL GUI)                                                │
│  ┌──────────┐ ┌──────────┐ ┌────────┐ ┌────────────────────────┐ │
│  │ ThingsCloud│ │  core    │ │ screen │ │    nav/card            │ │
│  │ WiFi/MQTT │ │ 主循环    │ │ 锁屏    │ │    卡片轮播            │ │
│  ├──────────┤ ├──────────┤ ├────────┤ ├──────────┬─────────────┤ │
│  │ FreeRTOS │ │ app      │ │ res    │ │ Settings │   Music      │ │
│  │ lcd_task │ │ 设置/音乐 │ │图片/字  │ │  WiFi    │  播放器+选歌  │ │
│  │ C0       │ │ /串口    │ │库/Lottie│ │  亮度    │  音量同步     │ │
│  └──────────┘ └──────────┘ └────────┘ └──────────┴─────────────┘ │
├───────────────────────────────────────────────────────────────────┤
│  service 层 — C++ 业务服务                                          │
│  ┌────────┐ ┌──────┐ ┌──────┐ ┌─────┐ ┌────────┐                │
│  │ audio  │ │  ui  │ │cloud │ │input│ │ network │                │
│  │ 播放器  │ │ 入口  │ │ MQTT │ │按键  │ │ TCP/IP │               │
│  └────────┘ └──────┘ └──────┘ └─────┘ └────────┘                │
│         ┌──────────────────────────────────────────────┐          │
│  system │ SystemRuntime · TaskManager · 服务注册表       │          │
│         └──────────────────────────────────────────────┘          │
├───────────────────────────────────────────────────────────────────┤
│  core 层 — 基类抽象 (无硬件依赖)                                    │
│  Lifecycle · EventBus · ConfigStore · 日志宏                       │
├───────────────────────────────────────────────────────────────────┤
│  board 层 — DeviceTree + Driver 引擎                               │
│  board.dts · 编译期设备表 · DRIVER_REGISTER · probe/remove          │
├───────────────────────────────────────────────────────────────────┤
│  drivers 层 — C 硬件驱动 (自注册)                                   │
│  ┌──────────┐ ┌────────────┐ ┌──────────┐ ┌──────────┐          │
│  │ st7789   │ │ max98357a  │ │ gpio_key │ │ ws2812   │          │
│  │ 显示     │ │ 音频功放   │ │ 按键     │ │ RGB LED  │          │
│  └──────────┘ └────────────┘ └──────────┘ └──────────┘          │
├───────────────────────────────────────────────────────────────────┤
│  hal 层 — 纯 C 函数指针表 (ESP-IDF 隔离层)                         │
│  hal_gpio · hal_spi_bus · hal_i2s_bus · hal_pwm · hal_rmt_led    │
├───────────────────────────────────────────────────────────────────┤
│  algorithm 层 — 纯计算，零硬件依赖                                   │
│  buffer/FIFO · DSP/EQ (Q31 定点)                                   │
├───────────────────────────────────────────────────────────────────┤
│  ESP-IDF — FreeRTOS · LWIP · ESP-NETIF · MBEDTLS · DRIVERS        │
└───────────────────────────────────────────────────────────────────┘
```

依赖方向：`app → system → service → board → drivers → hal → ESP-IDF`, `core` 被 system/service/app 共享

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

检测流程：`KeyInput::process()` 调用 `gpio_key_scan()` → 时间戳消抖 → 上升沿/下降沿检测 → `get_pressed_gpio()` 读取

## 6. 模块职责

| 模块 | 解决什么问题 | 入→出 |
|------|-------------|-------|
| **app/lvgl** | 240×240 屏幕 GUI：锁屏、菜单、设置、音乐播放器、串口终端 | 按键事件 → 屏幕绘制 |
| **core/** | Lifecycle/EventBus 基类、日志宏、JSON 配置读取 | 零硬件依赖 |
| **system/** | 系统编排：SystemRuntime、TaskManager、服务注册表、驱动注册列表 | 初始化 → 服务生命周期 |
| **service/audio** | MP3 解码、功放控制、音量管理 | 用户操作 → I2S 输出 |
| **service/ui** | LVGL 主循环入口，函数指针桥接 app | 启动 → lvgl_main() |
| **service/cloud** | WiFi 连接、MQTT 协议、数据上报 | 云指令 → RGB LED |
| **service/input** | 按键 GPIO 轮询扫描、消抖、LVGL 回调用 | GPIO → 按键事件 |
| **board/** | board.dts 编译期 DTS → 静态设备表 + probe 顺序生成 | DTS → 编译期表 → probe |
| **hal/** | ESP-IDF 函数指针封装（gpio/spi/i2s/pwm/rmt_led），驱动不直接调 IDF | 操作请求 → 硬件 |
| **drivers/** | 具体硬件驱动：st7789 显示、max98357a 功放、gpio_key 按键、ws2812 LED | 驱动注册 → probe |
| **algorithm/dsp** | 多段 EQ 滤波、音量平滑（Q31 定点），纯算法不依赖硬件 | 音频流 → 滤波后音频流 |

## 7. DeviceTree 硬件描述 (编译期)

项目使用 **编译期 MCU Lite DTS** (`components/board/board.dts`) 作为唯一硬件配置入口。  
构建时由 `tools/dtc-lite.py` 解析为 C 静态表，嵌入式运行，无运行时解析开销。

```dts
/dts-v1/;
/ {
    model = "sound_dsp_board_v1";
    aliases { display0 = &lcd0; i2s-bus = &i2s_audio0; };
    chosen { display = &lcd0; audio-out = &speaker_amp0; };
    soc {
        spi2: spi@2 { compatible = "esp32,spi-bus"; mosi = <5>; sclk = <4>; };
        i2s_audio0: i2s@0 { compatible = "esp32,i2s-bus"; ws = <13>; bclk = <12>; };
    };
    display {
        lcd0: st7789@0 { compatible = "sitronix,st7789"; depends-on = <&spi2>; ... };
    };
};
```

### 工作流程

```
board.dts ──▶ dtc-lite.py (编译期) ──▶ board_nodes.h / board_devtable.c / board_probe.c
                                               │
                                        board_driver_probe_all()
                                               │
                                         设备就绪 (DEVICE_STATUS_PROBED)
```

- `board.dts` 在构建时由 `dtc-lite.py` 解析，生成 `board_nodes.h`（DEV_ID 枚举）、`board_devtable.c`（静态 device_t 表）、`board_probe.c`（probe 函数表 + 拓扑排序顺序）
- `DRIVER_REGISTER` 宏在驱动 .c 文件中声明兼容性，dtc-lite.py 编译期扫描匹配
- 生成代码无 strcmp、无运行时解析，probe 按依赖拓扑排序直接函数调用
- 服务层通过 `device_find("lcd0")` 或 `DEV_ID_LCD0` 枚举获取设备

**更换硬件只需改 `board.dts` 中的引脚号**，无需改 C 代码。

## 8. 开发规范

- **不擅自改代码**：有修改建议先说明理由再动手
- **新架构先问**：模块级变动必须先讨论
- **不构建多余抽象**：三个相似行比过早封装好
- **每次改完写 NOTICE**：关键踩坑点写入 `NOTICE.md`
- **文件标注日期**：所有 .html/.md 标注 `YYYY-MM-DD`
- **层级依赖约束**：

| 层级 | 不允许依赖 |
|------|-----------|
| board/ (DTS 核心) | 无上层模块 |
| hal/ | 无上层模块 |
| drivers/ | UI, app, service, lvgl |
| algorithm/ | UI, RTOS, HAL, 任何硬件 |
| service/ | 不直接调 HAL/ESP-IDF（全通过 driver） |
| core/ | 无上层模块 |

- **C++ 规范**：禁用 RTTI/异常，跨文件回调用静态成员函数或函数指针
- **新增驱动流程**：`components/drivers/` 下创建 → 加 `DRIVER_REGISTER` → `board_driver_list.c` 加一行

## 9. DSP / EQ

多段均衡器，Biquad 级联，Q31 定点实现：

```
        b₀ + b₁·z⁻¹ + b₂·z⁻²
H(z) = ──────────────────────
        a₀ + a₁·z⁻¹ + a₂·z⁻²
```

- 可配置 N 段 EQ（典型 5 段）
- Q31 定点运算（RBJ 系数 → 定点转换）
- 音量平滑：一阶 IIR，Q15 系数 `VOLUME_SMOOTH_ALPHA_Q15 = 33`

---

> 完整开发时间线、15+ 个关键坑及解决方案 → [`NOTICE.html`](./NOTICE.html)
