# 🏗️ Sound DSP Project (ESP32-S3)

> 面向对象、高解耦的嵌入式音频数字信号处理与图形交互系统。
>
> 经过 25 轮架构审计与重构 (IEC 61508 SIL 4 / ISO 26262 ASIL-D / FDA Class III)，已构建 7 层安全防御体系。

---

| 文档 | 说明 |
|------|------|
| [📄 **核心架构全景** →](docs/architecture.md) | 分层架构、DeviceTree、Capability Engine、内存布局等详细设计 |
| [📋 **踩坑全记录** →](NOTICE.md) | 架构演进史、疑难杂症根因分析与解决方案 |

---

## 1. 🛡️ 当前架构加固态

经过 25 轮逐层审计后，系统已建成 7 层纵深防御体系：

| 层次 | 机制 | 对抗目标 |
|------|------|----------|
| **L1** — OSAL_PANIC 硬件急停 | `system_safety_hardware_shutdown` | 运行时致命错误 |
| **L2** — EventBus 死锁检测 | 锁超时 → `enter_safe_state` | 互斥锁死锁 |
| **L3** — 服务初始化失败 | 6 个致命接入点 → Safe State | 硬件故障 (FPC/焊接) |
| **L4** — Bootloop 防护 | RTC_DATA_ATTR 计数器 ≥5 → 锁死 | 无限重启 Flash 烧穿 |
| **L5** — RTC 硬件看门狗 | 独立 32kHz 时钟, 8s 超时 | CPU/总线卡死 |
| **L6** — Flash 位腐烂巡检 | 32B chunk 后台 CRC 比对 | 电荷流失 / 位翻转 |
| **L7** — NMI 紧急标记 | 纯寄存器操作, `IRAM_ATTR` | BOD/电源跌落 |

### 关键质量指标

| 指标 | 数值 |
|------|------|
| 架构审计轮次 | 25 轮 |
| 修复缺陷 | 107+ 项 |
| 覆盖文件 | 85+ 文件 |
| 新建文件 | 20+ 文件 (dtc-lite, hal_force_stop, safe_state, ...) |
| 安全防御层 | 7 层 (L1-L7) |
| DMA 缓冲区对齐 | 64 bytes (ESP32-S3 Cache Line) |
| 静态 DRAM 占用 | ~26.5 KB (优化 15%) |
| 运行时堆分配 | 零 (全部 BSS 静态池) |
| 锁超时上限 | 500ms (无 WAIT_FOREVER) |
| 音频缓冲 | 32KB / 185ms (BSS 段, 100ms Flash 写覆盖) |

详见 [📋 踩坑全记录 →](NOTICE.md)

---

## 2. 🚀 核心选型：为什么是 ESP32-S3？

| 核心需求 | ESP32-C3 | ESP32-C6 | **ESP32-S3** |
|----------|----------|----------|--------------|
| PSRAM 扩展 (刚需) | ❌ 不支持 | ❌ 不支持 | ✅ Octal SPI 8MB |
| Lottie 动画渲染 (160KB 缓冲) | ❌ SRAM 仅 400KB | ❌ SRAM 仅 512KB | ✅ PSRAM 充足分配 |
| 240×240 圆屏 + LVGL 9.x | ⚠️ 单核性能勉强 | ⚠️ 单核性能勉强 | ✅ 双核 LX7 流畅驱动 |
| 音视频高频并发实时性 | ❌ 单核极易卡音 | ❌ 单核极易卡音 | ✅ UI 核 + 音频核 |
| 高刷屏通信带宽 | ❌ Quad SPI | ❌ Quad SPI | ✅ Octal SPI |
| DSP 算法 / 音频算力 | ❌ 标量运算 | ❌ 标量运算 | ✅ 向量指令集加速 |
| 外设扩展能力 | ❌ 无 USB-OTG | ❌ 无 USB-OTG | ✅ 原生 USB-OTG |

> **架构断言**：Lottie 复杂渲染对内存吞吐是海量的，C3/C6 无 PSRAM 直接出局。双核架构是实现 UI 渲染与音频解码物理隔离的唯一解。

---

## 2. 🧠 架构总览

系统采用 **7 层单向依赖模型**，从 HAL 到 App 严格分层：

```
app → system → service → capability → board → drivers → hal_if → ESP-IDF
                ↓                                         ↑
              media ──────────────────────────────────────┤
              algorithm (纯 DSP, 无硬件依赖)               │
              config                                       │
              core (Lifecycle, EventBus) ──────────────────┘
```

👉 [详见架构文档 → 系统层级总览](docs/architecture.md#-1-系统层级总览-system-hierarchy)

---

## 3. 🕹️ 物理按键映射

| 丝印 | GPIO | 交互语义 |
|------|------|----------|
| PREV | 17 | 焦点上移 / 减小参数 / 页面左滑 |
| NEXT | 16 | 焦点下移 / 增大参数 / 页面右滑 |
| ENTER | 3 | 唤醒解锁 / 确认执行 / 状态切换 |
| ESC | 46 | 取消操作 / 返回上一层级 |

> **⚠️ 已知局限**：LVGL 9.5 `LV_INDEV_TYPE_KEYPAD` 在 Group 内会吞噬 PREV/NEXT 事件。当前通过 `KeyInput::process()` 直接拦截 GPIO 轮询绕过（详见 [踩坑记录](NOTICE.md)）。

---

## 4. 🧩 模块职责速查

| 模块 | 职责 | 依赖限制 |
|------|------|----------|
| `app/` | LVGL GUI (锁屏/菜单/播放器/设置/串口) | service, system |
| `core/` | Lifecycle, EventBus, production_log, critical_data | **零硬件依赖** |
| `system/` | SystemRuntime, TaskManager, SafeState, WDT, Scrubber | core |
| `service/` | 业务逻辑 (audio/ui/cloud/input/network/protocol) | capability, core |
| `media/` | MP3 解码 + I2S 异步双缓冲输出管线 | board, hal_if |
| `capability/` | 硬件能力门面 (render/audio/input/led engine) | board, drivers |
| `board/` | DeviceTree 运行时 + VFS 核心 + config_store A/B Slot | drivers |
| `drivers/` | 外设驱动 (ST7789/MAX98357A/WS2812/gpio_key/light_sensor/pwm_bl) | hal_if, osal |
| `hal_if/` | 函数指针表 (ESP-IDF 隔离层) | **纯接口** |
| `soc_port_esp32/` | ESP32 平台 HAL 实现 | hal_if, osal |
| `osal/` | OS 抽象层 (task/mutex/time/log/memory/pool) | **纯接口** |
| `algorithm/` | DSP (Biquad 定点滤波, 多段 EQ) | **纯计算** |
| `config/` | 编译时开关 + 运行时 JSON 配置 | **零依赖** |

---

## 5. 📦 快速开始

```bash
# 1. 设置 ESP-IDF 环境 (v5.5+)
source $IDF_PATH/export.sh

# 2. 构建
idf.py build

# 3. 烧录
idf.py -p /dev/ttyACM0 flash

# 4. (可选) 监视串口
idf.py -p /dev/ttyACM0 monitor
```

> **注意**：ESP-IDF v5.5 在 MSYS2/Mingw 下会被拦截，请使用原生 cmd.exe 或 PowerShell 构建（详见 [踩坑记录](NOTICE.md#21-⚠️-esp-idf-v55-msys2mingw-不支持)）。

---

## 6. 📏 开发红线规范

| 原则 | 要求 |
|------|------|
| **沟通先行** | 改核心逻辑或引入新抽象前，必须讨论确认 |
| **适度封装** | 三个相似行 > 一个过早抽象。不设计未来需求 |
| **文档同步** | 踩坑必须写入 `NOTICE.md`，注明日期 |
| **C++ 极简** | 禁用 RTTI 与 try-catch。跨文件回调用静态函数或 C 指针 |
| **驱动扩展** | 2 步：丢入 `drivers/` + `DRIVER_REGISTER`；更新 Board 注册表 |
| **层级隔离** | 禁止下层调上层。drivers/ 不包含 UI/App/Service/LVGL 头文件 |

---

## 7. 📚 关键文档索引

| 文档 | 位置 | 内容 |
|------|------|------|
| 核心架构 | [docs/architecture.md](docs/architecture.md) | 分层模型、DeviceTree、Capability Engine、内存布局、DSP 算法 |
| 踩坑全记录 | [NOTICE.md](NOTICE.md) | 架构演进史、重大 Bug 根因、链接器/MSYS2 踩坑、内存优化 |
| 按键配置 | `tools/kconfig-vscode/README.md` | VS Code Kconfig 集成 |
| DSP 算法 | `components/algorithm/dsp/README.md` | EQ 滤波器系数说明 |
