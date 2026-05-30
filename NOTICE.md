# 📓 Sound DSP — 核心架构演进与踩坑全记录

> **版本记录与技术债务清理史**
> 本文档详尽记录了项目从基础原型向**强实时、零内存碎片、极致解耦的工业级/车规级架构**蜕变的全过程。

### 📑 快速索引

| 阶段 | 章节 |
|------|------|
| 🎨 UI/功能迭代 | [1. 主题系统](#-1-浅色深色双轨主题系统) · [2. LVGL 花屏修复](#-2-lvgl-帧缓冲内存分配修复-花屏卡死) · [3. 串口 FIFO](#-3-串口环形缓冲区标准库替换) · [4. 代码风格](#-4-全局括号风格规范化) · [5. UI 精调](#-5-ui-细节精调微操作) |
| 🏛️ 第一阶段重构 | [7. 凭据外移](#7-凭据硬编码外移脱敏) · [8. SPI 静态化](#8-spi-dma-缓冲静态化) · [9. 按键重构](#9-按键模块重构--lvgl-集成) · [10. WiFi 事件驱动](#10-wifi-事件驱动改造) · [11. BSP 规范化](#11-bsp-层接口绝对规范化) · [12. 依赖收敛](#12-架构依赖边界收敛) · [13. 内存优化](#13-极致内存优化-diram-bss-占用-88--31) |
| 🏗️ 第二阶段解耦 | [16. DeviceTree + Driver Model](#16-架构重建devicetree--驱动全解耦) · [17. Media + Capability](#17-边界划定media层--capability隔离) |
| 🐛 第三阶段踩坑 | [18. 串口重启](#18-未解决-pc-关闭串口时系统自动重启) · [19. 链接器符号吞噬](#19-链接器大坑纯-probe-表驱动的符号被吞噬) · [20. 状态机](#20-app-层状态机守卫) · [21. MSYS2](#21-esp-idf-v55-拦截-msys2-构建) |
| 🚀 第四阶段改造 | [22. Platform Driver 化](#22-hw_hal-全量-linux-platform-driver-化) · [23. OS/MCU 解耦](#23-osmcu-完全解耦-三层物理抽象) · [24. Goto 清理](#24-引入-linux-kernel-风格-goto-清理流) · [25. WS2812 剥离](#25-ws2812-物理剥离与-platform-data-注入) · [26. 错误码统配](#26-全局负数错误码-posix-error-codes-统配) · [27. 总线锁](#27-spi-父设备总线锁-多步事务原子性) · [28. 最终修复](#28-扫清最后的架构瑕疵-the-final-touch) |
| 🔒 第五阶段安全 | [29. IEC 61508 深度修复](#29-iec-61508--iec-62304-深度安全修复) |

---

## 🎨 1. 浅色/深色双轨主题系统
* **[新增]** `core/inc/theme.hpp` + `core/src/theme.cpp`
* **机制**：全局 `AppTheme` 枚举（`THEME_DARK` / `THEME_LIGHT`），提供 `theme_set()` / `theme_get()` 接口。
* **调色板**：封装 10 个语义化颜色 getter（如 `th_bg` / `th_card` / `th_text` / `th_accent` 等）。浅色采用柔和类 iOS 配色（`th_bg=0xE4E5E9`）。
* **替换**：全量清理了 `music_app`, `card_menu`, `serial_app` 等模块中的硬编码颜色，共计修改近 **90 处**。

> [!NOTE] 
> `th_text_sec()` 深浅主题保持一致（0x8E8E93）；日志等级颜色（INFO/WARN/ERROR）及图像资源不随主题变化，以保障辨识度。

## 🐛 2. LVGL 帧缓冲内存分配修复 (花屏/卡死)
* **[文件]** `core/src/lvgl_main.cpp`
* **[修改]** 新增 `lvgl_alloc_buf()`，强制优先从内部 RAM (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`) 分配 LVGL 绘制缓冲区。仅当 `USE_LVGL_PRAM=1` 时才回退至 PSRAM。
* **[根因]** ESP32-S3 的 PSRAM 通过 CPU Cache 访问。当 WiFi 触发 `bcn_timeout` 重连导致 Flash/NVS 操作时，Cache 会被短暂禁用，引发 PSRAM 帧数据损坏，最终导致屏幕花屏或系统卡死。

## ♻️ 3. 串口环形缓冲区标准库替换
* **[文件]** `serial_console.cpp`, `serial_console.hpp`
* **[修改]** 彻底废弃手写 Ring Buffer，改用 ESP-IDF 标准 `buffer.h` 中的 `FIFO_Type_Def` 及其生态函数（`fifo_init` / `fifo_push` / `fifo_pop`）。
* **[内存]** 缓冲区内存仍强制从 PSRAM 分配（`MALLOC_CAP_SPIRAM`），防止挤占宝贵的 DRAM。

## 🧹 4. 全局括号风格规范化
* **[重构]** 遍历全项目文件，将所有控制语句（`if` / `for` / `while` / `switch`）的左括号由同行风格统一修正为**换行大括号风格**，提升工业级代码的视觉一致性。

## 💅 5. UI 细节精调微操作

| 优化模块 | 细节描述 |
| :--- | :--- |
| **🎵 音乐碟片** | 背景固定为 `0x0E0F14`（深色唱片色），浅色模式下依然保留黑胶唱片质感。 |
| **💻 串口终端** | 浅色模式背景：`0xF0F1F4`；深色模式背景：`0x08090C`。 |
| **🎚️ 音量滑块** | 轨道固定纯白色 (`0xFFFFFF`) 配合半透明深色底，确保任意主题下的极佳对比度。 |
| **🖱️ 长按防抖** | 引入 `m_pending_enter` 标志位，彻底解决歌单长按 ENTER 导致的连发抖动问题。 |

---

# 🏛️ 历史架构变更 (第一阶段：基础重构与优化)

## 7. 凭据硬编码外移脱敏
* **[问题]** WiFi/MQTT 账号密码硬编码，存在安全与移植隐患。
* **[修改]** 引入 `main/Kconfig.projbuild`，将凭据纳入 ESP-IDF `menuconfig` 标准管理体系。运行时配置优先，空值则回退至 KConfig 默认值。

## 8. SPI DMA 缓冲静态化
* **[修改]** 修复了 `st7789.c` 中每次刷图都 `malloc`/`free` 4KB DMA 缓冲导致的严重堆碎片问题。改为在 `bsp_lcd_init()` 中**一次性静态分配**，全局复用。

## 9. 按键模块重构 + LVGL 集成
* **[修改]** `Button` 类全面重构：支持最多 8 组 GPIO，采用非阻塞 `process()` + 时间戳硬件级消抖。LVGL 输入回调直接挂载 `Button::get_pressed_gpio()`。

## 10. WiFi 事件驱动改造
* **[修改]** 废弃 `vTaskDelay` 的死等模式，全面拥抱 `EventGroup` + `IP_EVENT_STA_GOT_IP` 事件驱动状态机。

## 11. BSP 层接口绝对规范化
* **签名净化**：全面使用 typed pointer (如 `bsp_spi_handle*`) 替代 `void*`。
* **中断隔离**：驱动不再直接调用 BSP，改为通过 `GpioController` 的 `install_isr_service` 进行统一下发。
* **死代码清理**：移除空实现的 clock / amplifier 目录，净化工程树。

## 12. 架构依赖边界收敛
* **CMake 依赖**：严格划分 `REQUIRES` 与 `PRIV_REQUIRES`，斩断跨层依赖。
* **头文件脱敏**：`device_hal` 下的 7 个核心头文件彻底屏蔽底层 ESP-IDF 类型暴露。
* **内存泄漏修补**：补全 `bsp_tcp.c` / `bsp_mqtt.c` 异常分支的句柄释放闭环。

## 13. 极致内存优化：DIRAM .bss 占用 88% → 31%
* **[问题]** DIRAM 几乎爆满，LVGL 内存池与串口缓冲是罪魁祸首。
* **[修改]** 1. LVGL 切至 C 标准库 malloc，配合 `CONFIG_SPIRAM_USE_MALLOC=y` 自动重定向至 PSRAM。
  2. 串口大缓存强制打上 `EXT_RAM_BSS_ATTR` 标签丢入外部 RAM。
* **[结果]** `.bss` 占用从 **206 KB 骤降至 9 KB**！

---

# 🏗️ 第二阶段：Linux 驱动模型与组件解耦

## 16. 架构重建：DeviceTree + 驱动全解耦
* **[背景]** 彻底告别修改 C 代码换引脚的刀耕火种时代，引入 **JSON 格式 DeviceTree** 与 **Linux Platform Driver Model**。
* **[核心机制]** * 驱动层使用 `DRIVER_REGISTER` 宏实现自注册（包含 compatible / probe / remove）。
  * `board_driver_probe_all()` 引擎启动时遍历拓扑，按依赖链执行链式挂载。
  * `hal/` 层隔离 ESP-IDF 物理接口，上层外设驱动绝对平台无关。
* **[编译期 DTS 升级]** 运行时 JSON 解析升级为由 `tools/dtc-lite.py` 驱动的**编译期静态代码生成**，实现零运行时内存开销。

## 17. 边界划定：Media层 + Capability隔离
* **[重构]** * 新增 `media/` 层：收容 MP3 解码与缓冲逻辑，使其脱离纯算法层 (`algorithm/`)。
  * 新增 `capability/` 层：封锁 Service 层越级访问，强制通过设备树句柄 (`device_find`) 通信。
  * 解耦 `system/`：统一接管 `SystemRuntime::init`，收敛网络及底层启停权限。

---

# 🐛 第三阶段：疑难杂症与编译链踩坑

## 18. [未解决] PC 关闭串口时系统自动重启
* **[现象]** USB Serial/JTAG 断连时引发硬件 Panic 甚至 WDT 重启。
* **[根因]** TX FIFO 失去消费者，Host 断连状态无法通过寄存器可靠侦测。写入拥塞直接导致 CPU 挂起。
* **[现状]** 尝试屏蔽中断、探测 `txfifo_writable` 均未彻底根治，暂定挂起态规避。

## 19. ⚠️ 链接器大坑：纯 probe 表驱动的符号被吞噬
* **[现象]** 纯靠 `DRIVER_REGISTER` 注册的 `light_sensor` 驱动报 `undefined reference`，而其他四大驱动正常。
* **[根因]** GNU `ld` 归档提取规则：由于 `board_probe.c.obj` 尚未链接，probe 表指针不足以触发静态库 `.a` 中目标文件的提取。其他驱动正常是因为在旧代码中存在隐式的直接函数调用。
* **[解法]** 在 CMakeLists 中强制追加 `-u` (Undefined) 链接器标志，提前声明符号以拉取目标文件：
  ```cmake
  target_link_options(${COMPONENT_LIB} INTERFACE "-u" "board_driver_probe_light_sensor")
  ```

## 🤖 20. App 层状态机守卫 (State Machine Guard)
* **[修改]** 引入 `MusicState` 与 `MenuState` 严格枚举。
* **[收益]** 彻底封堵了异步 I/O (歌曲加载) 期间由于按键重入导致的状态崩溃，提供严格的生命周期守卫机制。

## ⚠️ 21. ESP-IDF v5.5 拦截 MSYS2 构建
* **[现象]** `idf.py build` 打印警告后直接退出。
* **[根因]** v5.5 源码写死拦截 `MSYSTEM` 环境变量。
* **[解法]** 临时修改 `idf.py` 脚本注入 `main()`，或改用 Windows 原生 CMD 环境执行。

---

# 🚀 第四阶段：工业级/车规级底层改造

## ⚙️ 22. hw_hal 全量 Linux Platform Driver 化
* **[重构]** 6 大物理外设 (GPIO/I2C/SPI/I2S/UART/RMT) 全部升级为带 probe()、remove() 与 fops 操作表的标准 Platform Driver。
* **[消费者]** ST7789、MP3、Serial 层不再直接调 `hal_init`，全盘改写为 `device_write()` 与 `device_ioctl()` 标准系统调用。

## 🧩 23. OS/MCU 完全解耦 (三层物理抽象)
* **[动机]** 斩断底层框架对 FreeRTOS 和 ESP-IDF 的强耦合，使其能够随时移植至 RT-Thread 或裸机。
* **[架构]** `hw_hal` 被精准切割为三层：
  * `hal_if/`：纯虚接口，可移植到任意 C 编译器。
  * `osal/`：OS 抽象层，接管时钟、延迟、互斥锁与内存池。
  * `soc_port_esp32/`：特有芯片实现层。
* **[极客细节]** GPIO 引入 `HAL_GPIO_FAST_PATH`，允许 ns 级时序操作绕过 VFS 函数指针表直接内联，兼顾解耦与极限性能。

## 🧹 24. 引入 Linux Kernel 风格 Goto 清理流
* **[重构]** 根除 15 个驱动函数中的 Inline Pool 释放乱象。
* **[规范]** 统一采用 `goto err_xxx;` 标签。资源清理严格遵循反向析构栈原则（后分配的先释放），极大降低了内存泄漏的风险。

## 💡 25. WS2812 物理剥离与 Platform Data 注入
* **[重构]** WS2812 驱动中彻底删除 `vfs_rmt.h`，改用标准 `device_write(parent, buf, 3)` 下发像素数据。
* **[内存]** 驱动实现零开销，GRB 显存由 Board 级通过 `platform_data` 静态注入。

## 🔢 26. 全局负数错误码 (POSIX Error Codes) 统配
* **[规范]** 封杀所有粗糙的 `return -1;`，全量替换为 `VFS_ERR_INVAL` / `VFS_ERR_IO` / `VFS_ERR_NOMEM` / `VFS_ERR_NOSPC`。异常追溯能力达到 OS 级别。

## 🔒 27. SPI 父设备总线锁 (多步事务原子性)
* **[重构]** 在 st7789 的核心刷图流中引入双层锁机制：
  ```c
  device_lock(priv->spi_dev); // [锁定物理总线] 防止其他 SPI 从机打断
  set_window(...) -> write_cmd(...) -> spi_write_chunked(...)
  device_unlock(priv->spi_dev);
  ```
* **[收益]** 从根源上扼杀了多任务高频读写复用总线导致的时序错乱花屏问题。

## ✅ 28. 扫清最后的架构瑕疵 (The Final Touch)

| 瑕疵 | 修复 |
|------|------|
| 🛡️ ST7789 多实例安全 | 一维 `line_buf` 升级为二维数组静态池，杜绝双屏显存覆写 |
| ✂️ 背光层跨界 | `pwm_bl` 从 ESP32 私有目录迁至通用 `drivers/display`，compatible 改为 `generic,pwm-backlight` |
| 🔒 WS2812 裸奔 | 补齐 `ws2812_ioctl` 缺失的 `device_lock/unlock`，保护纳秒级 RMT 时序 |
| 💾 初始化序列 | 定长结构体 → 扁平字节流解析，单命令参数不再受限，压缩 ROM 占用 |

## 🛡️ 29. IEC 61508 / IEC 62304 深度安全修复

### 29.1 FSM 原子性 (device_set_status)
- **[IEC 61508 §2.7.1]** `device_set_status` 的 read-check-write 不再裸奔。引入 `portMUX_TYPE s_status_lock` + `taskENTER_CRITICAL` 保护状态转换。
- **[fail-fast]** `device_tree_init` 中 mutex 创建失败不再静默降级，直接 `OSAL_PANIC` (log + abort)。

### 29.2 递归互斥锁 (Recursive Mutex)
- **[IEC 62304 §5.3.3]** `osal_freertos.c` 全面升级：`xSemaphoreCreateMutexStatic` → `xSemaphoreCreateRecursiveMutexStatic`，lock/unlock 同步改为 `TakeRecursive`/`GiveRecursive`。
- **[收益]** 消除 VFS 层与驱动内部同时 `device_lock` 时的自我死锁风险。

### 29.3 生命周期锁 (Lifecycle Locking)
- **[信任倒置修复]** `device_open`/`device_read`/`device_write`/`device_ioctl` 内部强制 `device_lock(dev)` → 执行 ops → `device_unlock(dev)`。应用层不再需要手动加锁。
- **[TOCTOU 消灭]** `device_open` 全程持锁，杜绝 Time-of-Check / Time-of-Use 竞态。线程 A 与 B 同时 open 同一设备不再导致 DMA/GPIO 双重配置。
- **[新增]** `device_close`/`device_suspend`/`device_resume` 生命周期函数，遵循同一持锁闭环规范。

### 29.4 状态机访问屏障 (Phantom I/O 阻断)
- **[IEC 61508 §3.2.3]** `device_can_access` 现在仅允许 `DEVICE_STATUS_RUNNING` 态通行。`PROBED` 态不再允许 read/write/ioctl，防止硬件未上电时的幽灵寄存器访问。
- **[影响]** 驱动 probe 期间对父设备的 I/O 需要通过提前 `device_open` 确保父设备进入 RUNNING 态。

### 29.5 IOCTL 边界强制 (sizeof Validation)
- **[IEC 61508 §3.1.5]** `device_ioctl` 签名增加 `size_t arg_len` 参数：`int device_ioctl(device_t* dev, int cmd, void* arg, size_t arg_len)`。
- `file_operation_t.ioctl` 同步更新，13 个驱动 + 15 个调用文件全部适配。
- 驱动内部可通过 `arg_len != sizeof(expected_type)` 校验参数完整性，杜绝 `void*` 越界。

### 29.6 Magic Number 类型安全
- **[IEC 62304 §5.5.4]** `light_sensor_priv_t` 增加 `magic` 字段 (`0x4C533332`)，probe 时写入，ioctl/read 入口校验。
- `light_sensor_read_arg_t` 增加 `magic` 字段 (`0x52454144`)，ioctl 做 struct 类型验证后提取 `value`。
- `light_remove` 增加硬件静默：发送 `ADC_CMD_STOP` 后清除 magic，fail-safe 状态确认。

### 29.7 ADC 安全关闭命令
- **[新增]** `vfs_adc.h`/`hal_adc.h` 增加 `ADC_CMD_STOP 0x71`。`adc.c` ioctl 处理（oneshot 模式为安全 no-op）。
- `light_sensor_remove` 调用 `ADC_CMD_STOP` 告知硬件层释放通道引用。

---

## 30. IEC 61508 最终重构：设备关键性 + I/O 超时强制
> **审查来源：** Auditor 级「最终重构指令」Mandate 4 & 5
> **核心目标：** 用 FSM 确保关键设备死亡触发系统复位 + 所有 I/O 通路携带超时参数

### 30.1 设备关键性等级 (Device Criticality)
- **[IEC 61508 §7.6.2.3]** 新增 `device_criticality_t` 枚举 (`DEVICE_CRIT_IGNORE` / `DEVICE_CRIT_WARNING` / `DEVICE_CRIT_FATAL`)。
- `device_node_t` 增加 `uint8_t criticality` 字段，由 dtc-lite.py 从 DTS `criticality = "fatal"|"warning"|"ignore"` 编译期生成。
- **FATAL 级设备**（SPI 总线、GPIO 控制器）probe 失败时触发 `OSAL_PANIC` → 系统复位。
- **WARNING 级**（显示、UART、I2C、按键）失败时记录告警，系统继续运行。
- **IGNORE 级**（LED、音频功放、光敏传感器）失败无声忽略。
- **[影响]** `board.dts` 中 14 个设备节点全部标注 criticality 属性。`board_driver.c` 新增 `handle_probe_failure()` 按等级分发。

### 30.2 I/O 超时强制 (Timeout Enforcement)
- **[IEC 61508 §3.2.2]** `file_operation_t.write/read` 签名增加 `uint32_t timeout_ms` 参数。
- `device_write()`/`device_read()` VFS 包装同步更新，透传 timeout_ms。
- 4 个 SoC 层驱动（uart/spi/rmt_led/i2s_bus）的 `fops_write` 函数适配新签名。
- 3 个调用方（st7789、ws2812、mp3）传入具体超时值（100~1000ms）。
- **[影响]**
  - `device.h` — file_operation_t 及 VFS API 声明更新
  - `board_device.c` — device_write/device_read 实现透传
  - `uart.c` / `spi.c` / `rmt_led.c` / `i2s_bus.c` — fops_write 签名适配
  - `st7789_driver.c` / `ws2812_driver.c` / `mp3.cpp` — 调用方传入 timeout_ms
  - `i2s_bus.c` 最大收益：硬编码 `1000` 替换为调用方传入的 timeout_ms

### 30.3 DTS Criticality 属性对照表
| 路径 | Label | Criticality | 说明 |
|------|-------|-------------|------|
| `/soc/spi@2` | spi2 | `fatal` | 显示总线，系统关键 |
| `/soc/gpio@0` | gpio | `fatal` | 几乎所有设备依赖 |
| `/soc/i2s@0` | i2s_audio0 | `warning` | 音频子系统 |
| `/soc/uart@0` | uart_debug | `warning` | 控制台输出 |
| `/soc/i2c@0` | i2c0 | `warning` | 仅光敏传感器 |
| `/soc/rmt@0` | rmt | `warning` | 仅 LED |
| `/soc/adc@1` | adc0 | `warning` | 仅光敏传感器 |
| `/display/display@0` | lcd0 | `warning` | 可无显示运行 |
| `/audio/amplifier@0` | speaker_amp0 | `ignore` | 音频放大，非必要 |
| `/input/gpio-keys@0` | buttons0 | `warning` | 人机交互 |
| `/pwm/pwm-backlight@0` | pwm_backlight | `ignore` | 背光，非必要 |
| `/leds/led@0` | rgb_led0 | `ignore` | 状态灯，装饰性 |
| `/sensors/light@0` | lights_sensor0 | `ignore` | 环境光，非必要 |
