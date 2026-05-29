# 架构

> 最后更新：2026-05-30

## 层级总览

```
┌────────────────────────────────────────────────────────────────────────────┐
│  app 层 (LVGL GUI)                                                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────────────────┐  │
│  │ ThingsCloud│ │  core    │ │  screen  │ │    nav/card                   │  │
│  │ WiFi/MQTT │ │ 主循环    │ │  锁屏     │ │    卡片轮播                   │  │
│  ├──────────┤ ├──────────┤ ├──────────┤ ├──────────┬───────────────────┤  │
│  │ FreeRTOS │ │ app      │ │  res     │ │ Settings │    Music           │  │
│  │ lcd_task │ │ 设置/串口 │ │ 图片/字体 │ │  WiFi    │    播放器+选歌      │  │
│  │ C0       │ │          │ │ Lottie   │ │  亮度    │    音量同步         │  │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┴───────────────────┘  │
├────────────────────────────────────────────────────────────────────────────┤
│  service 层 — C++ 业务服务                                                  │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌──────────┐ ┌───────────────┐   │
│  │  audio   │ │   ui     │ │  cloud    │ │  input   │ │   network     │   │
│  │  播放器   │ │  入口     │ │  MQTT     │ │  按键     │ │   TCP/IP      │   │
│  └──────────┘ └──────────┘ └───────────┘ └──────────┘ └───────────────┘   │
│                ┌──────────────────────────────────────────────┐            │
│       system   │ SystemRuntime · TaskManager · 服务注册表       │            │
│                └──────────────────────────────────────────────┘            │
├────────────────────────────────────────────────────────────────────────────┤
│  capability 层 — 服务与驱动之间的门面层 (C)                                  │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐             │
│  │  render    │ │   audio    │ │   input    │ │    led     │             │
│  │  engine    │ │   engine   │ │   engine   │ │   engine   │             │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘             │
│  每个 engine: device_find() → device_open() → device_ioctl()              │
├────────────────────────────────────────────────────────────────────────────┤
│  board 层 — DeviceTree 运行时 + VFS 包装                                    │
│  ┌────────────────────────────────────────────────────────────────────┐   │
│  │  device_find() · device_open() · device_ioctl() · device_read()    │   │
│  │  device_write()                                                    │   │
│  │  空安全分发：dev->ops->ioctl / dev->ops->read ...                   │   │
│  └────────────────────────────────────────────────────────────────────┘   │
│  ┌────────────────────────────────────────────────────────────────────┐   │
│  │  board_device.c — 运行时 device_t 实例、属性访问器                   │   │
│  │  board_driver.c — 按拓扑序 probe、DRIVER_REGISTER 注册              │   │
│  └────────────────────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────────────────────┤
│  drivers 层 — C 硬件驱动 (全部 static, VFS ops table)                      │
│  ┌──────────┐ ┌────────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐  │
│  │  st7789  │ │ max98357a  │ │ gpio_key │ │ ws2812   │ │ light_sensor │  │
│  │  显示屏   │ │  音频功放   │ │  按键     │ │ RGB LED  │ │  光敏电阻    │  │
│  └──────────┘ └────────────┘ └──────────┘ └──────────┘ └──────────────┘  │
│       │            │              │            │              │           │
│       └────────────┴──────────────┴────────────┴──────────────┘           │
│                         每个驱动:                                           │
│  static const file_operation_t xxx_fops = { .init, .ioctl /*, .read */ };  │
│  DRIVER_REGISTER(xxx, "compat,string", probe, remove);                     │
│  dev->ops = &xxx_fops;  /* probe 中设置 */                                  │
├────────────────────────────────────────────────────────────────────────────┤
│  hal 层 — C 函数指针表 (ESP-IDF 隔离层)                                     │
│  hal_gpio · hal_spi_bus · hal_i2s_bus · hal_pwm · hal_rmt_led            │
│  驱动不直接调 ESP-IDF，全部通过 HAL 函数表                                  │
├────────────────────────────────────────────────────────────────────────────┤
│  algorithm 层 — 纯计算，零硬件依赖                                           │
│  buffer/FIFO · DSP/EQ (Q31 定点)                                           │
├────────────────────────────────────────────────────────────────────────────┤
│  ESP-IDF — FreeRTOS · LWIP · ESP-NETIF · MBEDTLS · DRIVERS                │
└────────────────────────────────────────────────────────────────────────────┘
```

**依赖方向**：`app → system → capability → board → drivers → hal → ESP-IDF`
**共享层**：`core` (Lifecycle, EventBus, ConfigStore) — 无硬件依赖，被 system/service/app 共用

---

## DeviceTree + VFS 模型

项目使用编译期 mini DeviceTree + Linux 风格的 VFS 操作表。

```
board.dts ──▶ dtc-lite.py (编译期) ──▶ board_nodes.h / board_devtable.c / board_probe.c
                                                    │
                                             board_driver_probe_all()
                                                    │
                                         device_ioctl() / device_read() / device_write()
```

### 两阶段设备模型

| 阶段 | 类型 | 存放位置 | 说明 |
|------|------|----------|------|
| 编译期 | `device_node_t` | `.rodata` | name, compatible, props (key/value), 依赖列表 |
| 运行时 | `device_t` | `.bss` (s_devices[]) | 节点指针, 状态, priv_data, ops 表指针 |

`device_node_t` 由 `dtc-lite.py` 从 `board.dts` 生成，全 `const`，零运行时解析。  
`device_t` 在 `device_tree_init()` 中从编译期节点表初始化。

### file_operation_t (VFS 操作表)

```c
typedef struct file_operation {
    int8_t (*init) (device_t* dev);
    int8_t (*open) (device_t* dev, void* arg);
    int8_t (*write)(device_t* dev, const void* buffer, size_t len);
    int8_t (*read) (device_t* dev, void* buffer, size_t len);
    int8_t (*ioctl)(device_t* dev, int cmd, void* arg);
} file_operation_t;
```

每个驱动定义 `static const file_operation_t`，在 probe 中赋值 `dev->ops = &xxx_fops`。  
所有驱动函数均为 `static` — 对外只暴露 ioctl 命令宏。

### VFS 包装函数

```c
int device_open(device_t* dev, void* arg);     // 调用 dev->ops->open
int device_ioctl(device_t* dev, int cmd, void* arg);  // 调用 dev->ops->ioctl
int device_read(device_t* dev, void* buf, size_t len);
int device_write(device_t* dev, const void* buf, size_t len);
```

所有包装函数空安全 (检查 `dev && dev->ops && dev->ops->func`)。

### 驱动注册

```c
#define DRIVER_REGISTER(name, compat, probe_fn, remove_fn)

// 示例：
DRIVER_REGISTER(max98357a, "maxim,max98357a", max98357a_probe, max98357a_remove);
```

`dtc-lite.py` 在编译期扫描所有 `.c` 文件，提取 `DRIVER_REGISTER` 条目，  
按依赖拓扑排序生成 probe/remove 函数表。

---

## Capability Engine (能力引擎)

在 service 和 driver 之间有一层薄门面层：

| Engine | 设备 | 关键操作 |
|--------|------|----------|
| render_engine | `lcd0` (st7789) | flush, fill_screen, set_backlight, get_display_size |
| audio_engine | `speaker_amp0` (max98357a) | play, set_volume, set_enable |
| input_engine | `buttons0` (gpio_key) | scan, get_key_count |
| led_engine | `rgb_led0` (ws2812) | set_color, set_brightness, off |

每个 engine 内部执行 `device_find("name")` → `device_open()` → `device_ioctl()`。  
service 从不包含驱动头文件，也不直接调用驱动函数。

---

## 内存布局

```
┌──────────────────────────────────────┐
│  内部 SRAM ~512KB                    │
│  ├─ FreeRTOS 内核 + 任务栈           │
│  ├─ LVGL draw buffer + TLSF          │
│  ├─ lwIP 网络缓冲                    │
│  └─ WiFi 栈                          │
├──────────────────────────────────────┤
│  PSRAM 8MB                           │
│  ├─ LVGL 显示缓冲                    │
│  ├─ Lottie 渲染缓冲                  │
│  ├─ LCD 任务栈                       │
│  ├─ 串口环形缓冲                     │
│  └─ DSP / 音频工作内存               │
└──────────────────────────────────────┘
```

---

## UI 导航流

```
LockScreen ─▶ CardMenu ─▶ Settings / Music / Serial
      ▲                        │
      └──────── ESC ───────────┘
```

- **Layer 1**：锁屏 — 全屏灰紫 + 状态栏 (WiFi/蓝牙/电量/时间)，ENTER → Layer 2
- **Layer 2**：3 卡横向轮播 (设置/音乐/串口)，ENTER → Layer 3，ESC → Layer 1
- **Layer 3**：功能页 — 设置 (WiFi/蓝牙/MQTT/亮度)、音乐 (播放器+选歌)、串口 (终端/HEX/日志)

---

## 模块职责

| 模块 | 职责 | 依赖 |
|------|------|------|
| **app/lvgl** | 240×240 GUI：锁屏、菜单、设置、音乐播放器、串口 | service, system |
| **core/** | Lifecycle/EventBus/ConfigStore 基类、日志宏 | (无) |
| **system/** | SystemRuntime, TaskManager, 服务注册表 | core |
| **service/audio** | MP3 解码、功放控制、音量管理 | capability, core |
| **service/ui** | LVGL 主循环入口，函数指针桥接 app | capability, core |
| **service/cloud** | WiFi 连接、MQTT 协议、数据上报 | capability, core |
| **service/input** | GPIO 按键扫描、消抖、LVGL 回调用 | capability, core |
| **capability/** | 门面层 — render/audio/input/led 引擎 | board, drivers |
| **board/** | DTS 编译期表、device_t 运行时、VFS 包装 | drivers |
| **drivers/** | 硬件驱动：st7789, max98357a, gpio_key, ws2812, light_sensor | hal |
| **hal/** | ESP-IDF 函数指针封装 (gpio/spi/i2s/pwm/rmt_led) | ESP-IDF |
| **algorithm/dsp** | 多段 EQ、音量平滑 (Q31 定点) | (无) |

---

## 关键设计原则

- **无循环依赖**：依赖方向严格向下
- **驱动不直接调 ESP-IDF**：全部通过 HAL 函数表
- **驱动函数不对外暴露**：全部 `static`，通过 `device_ioctl()` 访问
- **无运行时 DTS 解析**：`board.dts` 编译期完全展开
- **Service 不包含驱动头文件**：通信链为 service → capability engine → VFS 包装
