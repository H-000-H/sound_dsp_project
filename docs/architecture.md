# 🏛️ Sound DSP Project — 核心架构全景解析

> **面向对象、极致解耦的嵌入式硬软件一体化架构白皮书**
>
> ⏱️ **最后更新**：2026-05-31
> 📌 **核心宗旨**：单向依赖、静态内存、物理与逻辑绝对隔离

---

## 🧭 1. 系统层级总览 (System Hierarchy)

项目采用严密的模块化分层演进模型。数据与控制流严格遵循**单向垂直向下**的原则，彻底杜绝循环依赖。

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│  app 层 (LVGL GUI)                                                               │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┬──────────────────┐         │
│  │ ThingsCloud│ │  core    │ │  screen  │ │ Settings │    Music          │         │
│  │ WiFi/MQTT │ │ 主循环    │ │  锁屏    │ │  WiFi    │    播放器+选歌    │         │
│  ├──────────┤ ├──────────┤ ├──────────┤ │  亮度    │    音量同步       │         │
│  │ FreeRTOS │ │ app      │ │  res     │ └──────────┴──────────────────┘         │
│  │ lcd_task │ │ 设置/串口 │ │ 图片/字体 │                                        │
│  │ C0       │ │          │ │ Lottie   │     nav/card (卡片轮播)                  │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────────────────────────────┤
│  service 层 — C++ 业务服务                                                       │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌──────────┐ ┌──────────────┐          │
│  │  audio   │ │   ui     │ │  cloud    │ │  input   │ │   network    │          │
│  │  播放器   │ │  入口    │ │  MQTT    │ │  按键    │ │   TCP/IP     │          │
│  └──────────┘ └──────────┘ └───────────┘ └──────────┘ └──────────────┘          │
│                ┌────────────────────────────────────────────────────┐             │
│       system   │ SystemRuntime · TaskManager · 服务注册表             │             │
│                └────────────────────────────────────────────────────┘             │
├──────────────────────────────────────────────────────────────────────────────────┤
│  capability 层 — 服务与驱动之间的门面层 (C)                                        │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐                    │
│  │  render    │ │   audio    │ │   input    │ │    led     │                    │
│  │  engine    │ │   engine   │ │   engine   │ │   engine   │                    │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘                    │
│  每个 engine: device_find() → device_open() → device_ioctl()                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│  board 层 — DeviceTree 运行时 + VFS 包装                                          │
│  ┌──────────────────────────────────────────────────────────────────────┐        │
│  │  device_find() · device_open() · device_ioctl() · device_read()      │        │
│  │  device_write() · device_lock() · device_unlock()                    │        │
│  │  空安全分发：dev->ops->ioctl / dev->ops->read ...                     │        │
│  └──────────────────────────────────────────────────────────────────────┘        │
│  ┌──────────────────────────────────────────────────────────────────────┐        │
│  │  board_device.c — 运行时 device_t 实例、属性访问器                     │        │
│  │  board_driver.c — 按拓扑序 probe、DRIVER_REGISTER 注册                │        │
│  └──────────────────────────────────────────────────────────────────────┘        │
├──────────────────────────────────────────────────────────────────────────────────┤
│  drivers 层 — C 硬件驱动 (全部 static, VFS ops table)                              │
│  ├─ display/st7789_driver.c    显示驱动 (ST7789)                                   │
│  ├─ display/pwm_backlight.c    PWM 背光驱动                                        │
│  ├─ audio/max98357a_driver.c   音频功放驱动                                        │
│  ├─ input/gpio_key_driver.c    按键驱动                                            │
│  ├─ led/ws2812_driver.c        RGB LED驱动                                        │
│  └─ sensor/light_sensor_driver.c  光敏传感器驱动                                   │
│                                                                                    │
│  每个驱动: static const file_operation_t xxx_fops = { .init, .ioctl, ... };         │
│           DRIVER_REGISTER(xxx, "compat,string", probe, remove);                    │
│           dev->ops = &xxx_fops;  /* probe 中设置 */                                │
├──────────────────────────────────────────────────────────────────────────────────┤
│  soc_port_esp32 层 — ESP32 平台 HAL 实现 (ESP-IDF)                                  │
│  spi · i2c · uart · pwm · gpio · rmt_led · i2s_bus · adc                          │
│  每个模块: hal_xxx_init_struct() + _impl 函数 + DRIVER_REGISTER                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│  hal_if 层 — 纯接口定义 (函数指针结构体 + ioctl 宏)                                  │
│  hal_gpio.h · hal_spi_bus.h · hal_i2s_bus.h · hal_pwm.h · hal_rmt_led.h           │
│  纯头文件，无实现代码，可移植到任意 C 编译器                                          │
├──────────────────────────────────────────────────────────────────────────────────┤
│  osal 层 — OS 抽象 (task/mutex/time/log/memory)                                    │
│  osal.h → osal_freertos.c (FreeRTOS 后端)                                          │
│  换 OS 只需新建 osal_rtthread.c / osal_baremetal.c                                 │
├──────────────────────────────────────────────────────────────────────────────────┤
│  algorithm 层 — 纯计算，零硬件依赖                                                   │
│  buffer/FIFO · DSP/EQ (Q31 定点 Biquad 滤波器)                                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│  ESP-IDF — FreeRTOS · LWIP · ESP-NETIF · MBEDTLS · DRIVERS                        │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 依赖方向

```
app → system → service → capability → board → drivers → hal_if
                ↓                                         ↑
              media ──────────────────────────────────────┤
              algorithm (纯 DSP, 无硬件依赖)               │
              config (运行时 JSON 配置)                     │
              core (Lifecycle, EventBus) ──────────────────┘
                                                          │
              soc_port_esp32 ─────────── 使用 ────────────┤
              osal ──────────────────── 使用 ─────────────┤
```

### 层级约束

| 层级 | 禁止依赖 |
|------|----------|
| `board/`, `hal_if/`, `core/` | 任何上层模块 |
| `drivers/` | UI、App、Service、LVGL |
| `service/` | 驱动、HAL、ESP-IDF (全通过 capability) |
| `system/` | App、UI |
| `algorithm/` | RTOS、HAL、任何硬件接口 |
| `media/` | Service、App、UI |

---

## 🌳 2. DeviceTree + VFS 模型

项目全面拥抱 Linux 驱动哲学，采用**编译期 mini DeviceTree** 结合 VFS 操作表。改外设只需改 `board.dts`，无需动 C 代码。

### 构建与解析工作流

```text
[board.dts]
    │
    ▼
dtc-lite.py (编译期 Python 解析器)
    │
    ├── board_nodes.h        (DEV_ID 枚举 + device_node_t 只读表)
    ├── board_devtable.c     (.rodata 设备节点数组)
    └── board_probe.c        (按拓扑序排序的 probe 函数指针表)
    │
    ▼
board_driver_probe_all()    (启动时按序 probe)
    │
    ▼
device_find() / device_ioctl() / device_write()    (运行时 VFS 访问)
```

### 两阶段设备模型

| 阶段 | 数据类型 | 内存段 | 职责 |
|------|----------|--------|------|
| 编译期 | `device_node_t` | `.rodata` (只读) | 节点名、Compatible、KV 属性、依赖拓扑 |
| 运行时 | `device_t` | `.bss` | 节点指针、运行状态、priv_data、Ops 函数表 |

> `device_node_t` 全部 `const`，零运行时解析开销。`device_t` 开机时通过 `device_tree_init()` 极速装载。

### VFS 操作表

```c
typedef struct file_operation {
    int8_t (*init) (device_t* dev);
    int8_t (*open) (device_t* dev, void* arg);
    int8_t (*write)(device_t* dev, const void* buffer, size_t len);
    int8_t (*read) (device_t* dev, void* buffer, size_t len);
    int8_t (*ioctl)(device_t* dev, int cmd, void* arg);
} file_operation_t;
```

分发函数自动执行空安全检查（`dev && dev->ops && dev->ops->ioctl`），拦截非法解引用。

### 驱动自注册宏

```c
DRIVER_REGISTER(max98357a, "maxim,max98357a", max98357a_probe, max98357a_remove);
```

编译期 `dtc-lite.py` 自动扫描 `.c` 文件提取注册信息，按硬件依赖树生成安全 Probe/Remove 序列。

> **⚠️ 链接注意**：纯靠 probe 表激活的驱动（无其他代码直接调用）需加 `-Wl,--undefined=symbol` 防止被归档器吞掉（详见 [NOTICE.md#19](../NOTICE.md#19-⚠️-链接器大坑纯-probe-表驱动的符号被吞噬)）。

---

## 🛡️ 3. Capability Engine (硬件能力门面层)

隔离业务与驱动的最后一道防火墙。Service 层绝对禁止包含驱动头文件或直接调底层函数。

| Engine | 绑定设备 | 暴露操作 |
|--------|----------|----------|
| `render_engine` | `lcd0` (ST7789) | flush, fill_screen, set_backlight, get_display_size |
| `audio_engine` | `speaker_amp0` (MAX98357A) | play, set_volume, set_enable |
| `input_engine` | `buttons0` (gpio_key) | scan, get_key_count |
| `led_engine` | `rgb_led0` (WS2812) | set_color, set_brightness, off |

**通信链路**：Engine 内部封装 `device_find("name") → device_open() → device_ioctl()`

---

## 🧠 4. 确定性内存布局

严格的静态规划，彻底压榨 ESP32-S3 性能，告别碎片化死锁。

```text
┌────────────────────────────────────────────────────────────┐
│  内部高速 SRAM (Internal SRAM) ~512KB                       │
│  ├─ FreeRTOS 内核 + 核心任务栈                     ~80KB   │
│  ├─ LVGL Draw Buffer (TLSF 算法缓冲)              ~140KB   │
│  ├─ lwIP 网络协议栈底层缓冲                        ~60KB   │
│  ├─ WiFi 驱动与协议栈                              ~80KB   │
│  └─ 系统预留与其他                                  余量    │
├────────────────────────────────────────────────────────────┤
│  外部扩展 PSRAM 8MB                                         │
│  ├─ LVGL Display Buffer (双缓冲)                  ~192KB   │
│  ├─ Lottie 矢量动画渲染专用缓冲                    160KB   │
│  ├─ LCD 刷新任务栈                                  8KB   │
│  ├─ 串口收发环形缓冲                               ~64KB   │
│  └─ DSP / EQ 音频算法工作区                      ~7.5MB   │
└────────────────────────────────────────────────────────────┘
```

### 优化收益

| 优化项 | 改前 | 改后 | 收益 |
|--------|------|------|------|
| Lottie 缓冲区 | `.bss` 段 SRAM | PSRAM 动态分配 | 释放 160KB SRAM |
| 显示缓冲区 | `MALLOC_CAP_DMA` | `MALLOC_CAP_SPIRAM` | 56KB 移出 DRAM |
| `LV_MEM_SIZE` | 32KB | 128KB | 解决复杂绘制死锁 |
| `dram0.bss` | 9,344 Bytes | 5,376 Bytes | 占用骤降 42% |
| 总静态 DRAM | ~31.3 KB | ~26.5 KB | 整体优化 15% |

---

## 🕹️ 5. UI 交互导航流

```text
 🔒 LockScreen ───[ENTER]──▶ 📋 CardMenu ───[ENTER]──▶ ⚙️ Settings / 🎵 Music / 💻 Serial
        ▲                                                         │
        └───────────────────────────[ESC]─────────────────────────┘
```

| 层级 | 内容 | 交互 |
|------|------|------|
| Layer 1 (锁屏) | 全屏紫灰底色 + 状态栏 (WiFi/蓝牙/电量/时间) | ENTER 解锁 |
| Layer 2 (菜单) | 3 卡横向阻尼轮播 (设置/音乐/串口) | PREV/NEXT 切换, ENTER 确认 |
| Layer 3 (业务) | 沉浸式功能页 | ESC 返回 |

---

## 🧩 6. 核心模块职责全览

| 模块 | 核心职责 | 外部依赖限制 |
|------|----------|-------------|
| `app/lvgl` | 240×240 GUI (锁屏/菜单/音乐/设置/串口终端) | service, system |
| `core/` | Lifecycle / EventBus / ConfigStore / Log | **绝对零依赖** |
| `system/` | SystemRuntime / TaskManager / 服务注册表 | core |
| `service/audio` | MP3 帧解码、数字音量平滑控制、I2S 功放 | capability, core |
| `service/ui` | LVGL 主循环守护、App 层注册 | capability, core |
| `service/cloud` | WiFi 态管理 / MQTT 通信 / 遥测上报 | capability, core |
| `service/input` | GPIO 按键轮询扫描、去抖、LVGL 键值注入 | capability, core |
| `media/` | MP3 解码器封装 + I2S 输出 + EQ 滤波 | board, hal_if, core, algorithm |
| `capability/` | 硬能力门面层，原子动作接口 | board, drivers |
| `board/` | DTS 编译期结构体 + 运行时 device_t + VFS 路由 | drivers |
| `drivers/` | 外设驱动 (ST7789/MAX98357A/WS2812/gpio_key/light_sensor/pwm_backlight) | hal_if, osal |
| `soc_port_esp32/` | ESP32 平台 HAL 实现 (SPI/I2C/UART/PWM/GPIO/RMT/I2S/ADC) | hal_if, osal |
| `hal_if/` | 函数指针表接口定义 (纯头文件, 零实现) | **纯接口** |
| `osal/` | OS 抽象层 (task/mutex/time/log/memory) | **纯接口** |
| `config/` | 编译时开关 + 运行时 JSON 配置 | **零依赖** |
| `algorithm/dsp/` | Biquad 定点滤波、多段 EQ | **纯计算** |

---

## 🎛️ 7. DSP 核心算法

### 多段级联 EQ

基于 Biquad（双二阶）滤波器的 N 段可配置均衡器（典型 5 段）。系数转换为 Q31 定点格式，压榨 ESP32-S3 向量计算资源。

### Biquad 传递函数

```text
        b₀ + b₁·z⁻¹ + b₂·z⁻²
H(z) = ──────────────────────
        a₀ + a₁·z⁻¹ + a₂·z⁻²
```

### 抗爆音平滑

音量调节采用一阶 IIR 平滑（Q15 精度，`ALPHA_Q15 = 33`），消除数字增益跳变的 Pop-noise。

---

## 📏 8. 开发红线规范

| 原则 | 要求 |
|------|------|
| **沟通先行** | 改核心逻辑或引入新抽象前必须讨论 |
| **适度封装** | 三个相似行 > 一个过早抽象。不设计未来需求 |
| **文档同步** | 踩坑必须同步更新 `NOTICE.md`，注明日期 |
| **C++ 极简** | 禁用 RTTI 与 try-catch。跨文件回调用纯静态成员函数或 C 指针 |
| **驱动扩展** | 2 步：丢入 `drivers/` + `DRIVER_REGISTER` → 构建 |
| **层级隔离** | `board/` / `hal_if/` / `core/` 不依赖上层；`drivers/` 不含 UI/App/LVGL |

---

## 📚 附：关键文档索引

| 文档 | 位置 | 说明 |
|------|------|------|
| 项目总览 | [README.md](../README.md) | 选型理由、按键映射、快速开始、开发规范 |
| 踩坑全记录 | [NOTICE.md](../NOTICE.md) | 架构演进史、重大 Bug、链接器/MSYS2 踩坑 |
| DSP 算法 | `components/algorithm/dsp/README.md` | EQ 滤波器系数详细说明 |
