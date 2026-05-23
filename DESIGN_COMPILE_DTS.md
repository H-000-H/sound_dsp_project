# MCU Compile-time DeviceTree — 嵌入式架构设计书

> 目标：MCU 级编译期 DTS + Driver Model + Capability Runtime  
> 针对 ESP32 / STM32 / 轻量 RTOS 平台  
> 核心约束：编译期确定性、零运行时解析、依赖注入

---

## 1. MCU Lite DTS 格式定义

### 1.1 语法

设计一种 stripped-down DTS，使用 C 预处理器友好风格，但仍保持可读性。

```dts
// board.dts — MCU Lite DeviceTree

/dts-v1/;

/ {
    model = "ESP32S3 Audio Board v2";
    compatible = "espressif,esp32s3";

    chosen {
        console = &uart0;
        display = &st7789_dev;
        audio-out = &max98357a_dev;
    };

    aliases {
        i2s-bus = &i2s0;
        spi-display = &spi0;
    };

    /* ── SoC 外设 ── */
    soc {
        spi0: spi@0 {
            compatible = "espressif,hal-spi";
            mosi-pin = <11>;
            miso-pin = <12>;
            sclk-pin = <13>;
            cs-pin = <14>;
            freq = <40000000>;
        };

        i2s0: i2s@0 {
            compatible = "espressif,hal-i2s";
            ws-pin    = <4>;
            bclk-pin  = <5>;
            dout-pin  = <6>;
            din-pin   = <7>;
            sample-rate = <44100>;
        };

        uart0: uart@0 {
            compatible = "espressif,hal-uart";
            tx-pin = <43>;
            rx-pin = <44>;
            baud   = <115200>;
        };
    };

    /* ── 显示设备 ── */
    display {
        st7789_dev: st7789@0 {
            compatible = "sitronix,st7789v";
            depends-on = <&spi0>;          /* phandle 引用: 显式依赖 */
            width  = <240>;
            height = <240>;
            dc-pin = <3>;
            rst-pin = <4>;
            bl-pin  = <15>;
            rgb-order = "bgr";
            rotate = <0>;
            invert = <1>;
        };
    };

    /* ── 音频 ── */
    audio {
        max98357a_dev: max98357a@0 {
            compatible = "maxim,max98357a";
            depends-on = <&i2s0>;
            sdn-pin = <10>;
        };
    };

    /* ── 输入 ── */
    input {
        compatible = "gpio-key-collection";
        btn-next {
            compatible = "gpio-keys";
            pin = <17>;
            pressed-level = <0>;       /* 低电平按下 */
            debounce-ms = <50>;
            label = "next";
        };
        btn-prev {
            compatible = "gpio-keys";
            pin = <16>;
            pressed-level = <0>;
            debounce-ms = <50>;
            label = "prev";
        };
        btn-enter {
            compatible = "gpio-keys";
            pin = <3>;
            pressed-level = <0>;
            debounce-ms = <50>;
            label = "enter";
        };
        btn-esc {
            compatible = "gpio-keys";
            pin = <46>;
            pressed-level = <0>;
            debounce-ms = <50>;
            label = "esc";
        };
    };

    /* ── 指示器 ── */
    leds {
        compatible = "led-collection";
        rgb-led@0 {
            compatible = "worldsemi,ws2812";
            pin = <38>;
            led-count = <1>;
            depends-on = <&rmt0>;      /* RMT 是 WS2812 的依赖 */
        };
    };

    /* ── 存储 ── */
    storage {
        compatible = "storage-collection";
        flash@0 {
            compatible = "jedec,spi-nor";
            depends-on = <&spi1>;
            cs-pin = <48>;
            freq = <80000000>;
        };
    };
};
```

### 1.2 属性类型约束

| DTS 写法 | C 类型 | 说明 |
|----------|--------|------|
| `<100>` | `int` | 单整数值 |
| `<100 200 300>` | `int[]` | 整数数组 |  
| `"string"` | `const char*` | 字符串 |
| `<&label>` | `int` (phandle) | 节点引用，编译期解析为索引 |
| `true;` / `false;` | `bool` | 布尔值 |

### 1.3 禁止的 Linux DT 特性

| 特性 | 禁止原因 |
|------|----------|
| `#address-cells` / `#size-cells` | MCU 不需要复杂地址映射 |
| `interrupt-parent` / `interrupts` | MCU 中断绑定在 HAL 层处理 |
| `reg` / `ranges` | 非 memory-mapped 外设（SPI/I2C/UART） |
| `dtsi` 包含 | 统一单文件, 多板卡通过 python 条件选择 |
| overlays | 不支持 runtime overlay |
| binding schema | 不需要 yaml 验证 |

---

## 2. 编译期代码生成管道

### 2.1 Pipeline

```
board.dts
    │
    ▼
dtc-lite.py  ───▶  generated/
    │                   ├── board_nodes.h      设备节点索引枚举
    │                   ├── board_devtable.c    设备表（静态 const）
    │                   ├── board_probe.c       probe 调用表（按依赖排序）
    │                   ├── board_init.c        初始化序列
    │                   ├── board_handles.h     依赖注入句柄结构体
    │                   ├── board_aliases.h     别名宏定义
    │                   └── board_chosen.h      chosen 设备宏
    │
    ▼
C Compiler (GCC / Clang / xtensa-esp)
    │
    ▼
.o + .a → firmware
```

### 2.2 dtc-lite.py 职责

```python
# dtc-lite.py — MCU DeviceTree Compiler
# 输入: board.dts (+ 可选 board variant)
# 输出: generated/ 目录下 7 个 C 文件
# 执行阶段: CMake add_custom_command / PRE_BUILD

def process(board_dts_path, output_dir):
    # 1. 解析 DTS (递归下降 parser, 无外部依赖)
    tree = parse_dts(board_dts_path)

    # 2. 解析 phandle & alias
    resolve_phandles(tree)      # &label → 节点索引
    resolve_aliases(tree)       # aliases{} 段

    # 3. 依赖拓扑排序 (DFS)
    dep_order = topological_sort(tree)

    # 4. 生成代码
    generate_node_enum(tree, output_dir)     # board_nodes.h
    generate_devtable(tree, output_dir)       # board_devtable.c
    generate_probe_table(tree, dep_order)     # board_probe.c
    generate_init_sequence(tree, dep_order)   # board_init.c
    generate_handle_struct(tree, output_dir)  # board_handles.h
    generate_aliases(tree, output_dir)        # board_aliases.h
    generate_chosen(tree, output_dir)         # board_chosen.h
```

### 2.3 生成代码示例

**board_nodes.h** — 设备索引枚举：
```c
// generated/board_nodes.h — 自动生成
// 每个设备节点对应一个枚举值, 用于 phandle 索引

#ifndef BOARD_NODES_H
#define BOARD_NODES_H

typedef enum {
    DEV_ID_SPI0 = 0,
    DEV_ID_I2S0,
    DEV_ID_UART0,
    DEV_ID_ST7789_DEV,
    DEV_ID_MAX98357A_DEV,
    DEV_ID_BTN_NEXT,
    DEV_ID_BTN_PREV,
    DEV_ID_BTN_ENTER,
    DEV_ID_BTN_ESC,
    DEV_ID_RGB_LED_0,
    DEV_ID_FLASH_0,
    DEV_ID_COUNT              /* 总设备数 = 11 */
} device_id_t;

/* phandle 转枚举宏 */
#define DEV_HANDLE(label)     DEV_ID_##label
#define PHANDLE_DEV(ptr)      ((device_id_t)((uintptr_t)(ptr)))

#endif
```

**board_devtable.c** — 静态设备表：
```c
// generated/board_devtable.c — 自动生成

#include "board_nodes.h"
#include "device.h"             /* device_t 类型定义 */

/* ── 设备节点表 (完全静态, .rodata) ── */
static const char* const s_dev_names[DEV_ID_COUNT] = {
    [DEV_ID_SPI0]       = "spi0",
    [DEV_ID_I2S0]       = "i2s0",
    [DEV_ID_UART0]      = "uart0",
    [DEV_ID_ST7789_DEV] = "st7789@0",
    [DEV_ID_MAX98357A_DEV] = "max98357a@0",
    /* ... */
};

/* ── 设备属性表 (按设备索引映射) ── */
static const device_prop_t s_spi0_props[] = {
    { .key = "mosi-pin", .type = PROP_INT, .val = { .i = 11 } },
    { .key = "miso-pin", .type = PROP_INT, .val = { .i = 12 } },
    { .key = "freq",     .type = PROP_INT, .val = { .i = 40000000 } },
    { .key = NULL },  /* 哨兵 */
};

/* ── 设备节点描述数组 ── */
const device_t g_devices[DEV_ID_COUNT] = {
    [DEV_ID_SPI0] = {
        .name      = "spi0",
        .compatible= "espressif,hal-spi",
        .status    = DEVICE_DISABLED,   /* probe 后 → READY/ERROR */
        .dep_count = 0,
        .dep_ids   = NULL,
        .props     = s_spi0_props,
    },
    [DEV_ID_ST7789_DEV] = {
        .name      = "st7789@0",
        .compatible= "sitronix,st7789v",
        .status    = DEVICE_DISABLED,
        .dep_count = 1,
        .dep_ids   = (const device_id_t[]){ DEV_ID_SPI0 },
        .props     = s_st7789_props,
    },
    /* ... */
};

/* 访问函数 (device.h 接口实现) */
const device_t* board_device_get(device_id_t id) {
    return &g_devices[id];
}

int board_device_count(void) {
    return DEV_ID_COUNT;
}
```

**board_probe.c** — probe 表（按依赖拓扑排序）：
```c
// generated/board_probe.c — 自动生成, 按依赖拓扑排序

#include "board_nodes.h"
#include "driver.h"

/* ── 外部驱动 probe 函数声明 ── */
extern int drv_hal_spi_probe(device_t*);
extern int drv_hal_i2s_probe(device_t*);
extern int drv_hal_uart_probe(device_t*);
extern int drv_st7789_probe(device_t*);
extern int drv_max98357a_probe(device_t*);
extern int drv_gpio_keys_probe(device_t*);
extern int drv_ws2812_probe(device_t*);
extern int drv_spi_flash_probe(device_t*);

/* ── probe 调用表 (依赖拓扑顺序 = 索引顺序) ── */
/* 保证: probe[i] 的所有依赖在 probe[0..i-1] 中已 probe 完毕 */
const probe_entry_t g_probe_table[DEV_ID_COUNT] = {
    /* 按拓扑排序: 先 HAL, 再 drivers */
    [0] = { .dev_id = DEV_ID_SPI0,       .probe = drv_hal_spi_probe },
    [1] = { .dev_id = DEV_ID_I2S0,       .probe = drv_hal_i2s_probe },
    [2] = { .dev_id = DEV_ID_UART0,      .probe = drv_hal_uart_probe },
    [3] = { .dev_id = DEV_ID_ST7789_DEV, .probe = drv_st7789_probe },
    [4] = { .dev_id = DEV_ID_MAX98357A_DEV, .probe = drv_max98357a_probe },
    /* ... */
};
```

**board_handles.h** — 依赖注入句柄：
```c
// generated/board_handles.h — 自动生成

#ifndef BOARD_HANDLES_H
#define BOARD_HANDLES_H

#include "capability/display.h"
#include "capability/audio_out.h"
#include "capability/input.h"
#include "capability/storage.h"

/* ── 编译期确定的所有能力句柄 ── */
struct board_capabilities {
    display_capability_t*   display;        /* chosen 指定的主显示设备 */
    audio_output_t*         audio_out;      /* chosen 指定的音频输出 */
    input_collection_t*     input;          /* 按键集合 */
    indicator_t*            indicator;      /* 状态 LED */
    storage_t*              persistent;     /* 持久化存储 */
};

/* ── 编译期确定的所有 HAL 句柄 ── */
struct board_hal_handles {
    void* spi_bus[2];       /* spi0, spi1 — 由 HAL 层定义实际类型 */
    void* i2s_bus;
    void* uart;
};

#endif
```

**board_init.c** — 初始化序列：
```c
// generated/board_init.c — 自动生成

#include "board_nodes.h"
#include "board_devtable.h"
#include "board_probe.h"
#include "driver.h"

/* 初始化入口 - 按拓扑顺序 probe 所有设备 */
int board_init_all(void) {
    for (int i = 0; i < DEV_ID_COUNT; i++) {
        const probe_entry_t* entry = &g_probe_table[i];
        const device_t* dev = board_device_get(entry->dev_id);
        int ret = entry->probe((device_t*)dev);
        if (ret != 0) {
            /* 可选: 依赖容错 (如果依赖失败, 跳过依赖者) */
            return ret;
        }
        /* 标记设备为 READY */
        // device_set_status(dev, DEVICE_READY);
    }
    return 0;
}
```

### 2.4 多板卡支持

```python
# dtc-lite.py variant selection
# Usage: python dtc-lite.py board/board-{variant}.dts
# 每种板卡只有 .dts 文件不同, generated/ 输出不同

boards:
  board-esp32s3-audio.dts     # 当前开发板
  board-esp32c3-mini.dts      # C3 低成本变体
  board-stm32f4-disco.dts     # STM32 移植
```

CMake 选择：
```cmake
# CMakeLists.txt
set(BOARD_DTS "board/board-${BOARD_VARIANT}.dts")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/board_nodes.h
           ${CMAKE_CURRENT_BINARY_DIR}/generated/board_devtable.c
           ...
    COMMAND python3 dtc-lite.py ${BOARD_DTS} ${CMAKE_CURRENT_BINARY_DIR}/generated
    DEPENDS ${BOARD_DTS}
)
```

---

## 3. Driver Model

### 3.1 DRIVER_REGISTER 宏设计

```c
// driver.h — Driver 注册宏

/* ── 驱动描述符 ── */
typedef struct driver_desc {
    const char*         compatible;     /* 匹配字符串 */
    int                 (*probe)(device_t*);   /* 设备探测 */
    int                 (*remove)(device_t*);  /* 设备移除 */
} driver_desc_t;

/* ── 注册宏 (生成驱动描述符, 用于 Python 扫描) ── */
#define DRIVER_REGISTER(name, compat_str, probe_fn, remove_fn)      \
    static int probe_fn(device_t*);                                  \
    static int remove_fn(device_t*);                                 \
    const driver_desc_t _drv_desc_##name                             \
        __attribute__((section(".driver_desc_table")))               \
        __attribute__((used)) = {                                    \
        .compatible = compat_str,                                    \
        .probe  = probe_fn,                                          \
        .remove = remove_fn,                                         \
    }
```

### 3.2 Driver 实现示例

```c
// st7789_driver.c — 完整驱动程序

#include "device.h"
#include "driver.h"
#include "hal/hal_spi.h"        /* HAL 接口, 非 ESP-IDF */

/* ── 私有状态 ── */
typedef struct st7789_priv {
    hal_spi_bus_t*   spi;       /* HAL handle (由 parent probe 提供) */
    hal_gpio_t       dc;
    hal_gpio_t       rst;
    int              width;
    int              height;
} st7789_priv_t;

/* ── probe: 硬件初始化 ── */
static int st7789_probe(device_t* dev) {
    /* 1. 读取 DTS 属性 */
    int dc_pin, rst_pin, bl_pin, width, height, invert;
    device_read_prop(dev, "dc-pin",  &dc_pin);
    device_read_prop(dev, "rst-pin", &rst_pin);
    device_read_prop(dev, "width",   &width);
    device_read_prop(dev, "height",  &height);
    device_read_prop_bool(dev, "invert", &invert);

    /* 2. 获取 parent (spi0) 的 HAL handle */
    device_t* parent = device_get_dep(dev, 0);      /* depends-on = &spi0 */
    hal_spi_bus_t* spi = (hal_spi_bus_t*)device_get_hal(parent);
    if (!spi) return -1;

    /* 3. 分配私有状态 (静态池 or malloc) */
    st7789_priv_t* priv = &g_st7789_priv;           /* 静态分配, 无 malloc */
    priv->spi = spi;
    hal_gpio_init(&priv->dc,  dc_pin,  HAL_GPIO_OUTPUT);
    hal_gpio_init(&priv->rst, rst_pin, HAL_GPIO_OUTPUT);
    priv->width  = width;
    priv->height = height;

    /* 4. 硬件初始化序列 */
    hal_gpio_set(&priv->rst, 0);
    hal_timer_delay_ms(50);
    hal_gpio_set(&priv->rst, 1);
    hal_timer_delay_ms(120);
    st7789_init_commands(priv->spi, &priv->dc, invert);

    /* 5. 关联私有状态 */
    device_set_priv(dev, priv);
    return 0;
}

/* ── remove: 去初始化 ── */
static int st7789_remove(device_t* dev) {
    st7789_priv_t* priv = (st7789_priv_t*)device_get_priv(dev);
    if (priv) {
        hal_gpio_set(&priv->dc, 0);
        hal_gpio_deinit(&priv->dc);
        /* ... */
        device_set_priv(dev, NULL);
    }
    return 0;
}

/* ── 驱动注册 ── */
DRIVER_REGISTER(st7789, "sitronix,st7789v", st7789_probe, st7789_remove);
```

### 3.3 匹配机制 (编译期)

**核心设计**: 不进行运行时 compatible 字符串匹配。

Python 脚本 `dtc-lite.py` 同时扫描:
1. `board.dts` — 获取所有 `compatible` 属性值
2. 所有驱动 `.c` 文件中的 `DRIVER_REGISTER` — 获取 `compatible` 注册

脚本生成 `.c` 文件时，直接按设备顺序生成 probe 调用表，完全绕过了运行时匹配:

```
dtc-lite.py 读取 board.dts:
  → spi0: compatible = "espressif,hal-spi"
  → st7789@0: compatible = "sitronix,st7789v"

dtc-lite.py 扫描 drivers/*/*.c:
  → DRIVER_REGISTER(hal_spi, "espressif,hal-spi", ...)
  → DRIVER_REGISTER(st7789, "sitronix,st7789v", ...)

dtc-lite.py 生成 board_probe.c:
  → [0] = { .dev_id = DEV_ID_SPI0,  .probe = hal_spi_probe }
  → [1] = { .dev_id = DEV_ID_ST7789, .probe = st7789_probe }
    /* ↑ 直接函数调用, 零运行时开销 */
```

如果某个 `compatible` 在驱动中找不到对应注册，编译时报错:
```
ERROR: device 'st7789@0' has compatible 'sitronix,st7789v' but no driver registered for it
```

### 3.4 driver 私有状态管理（无 malloc）

```c
// driver_priv.h — 静态分配池

/* 每个 driver 声明自己的最大实例数 */
#define ST7789_MAX_INSTANCES    1
#define MAX98357A_MAX_INSTANCES 1
#define GPIO_KEYS_MAX_INSTANCES 4
#define WS2812_MAX_INSTANCES    2

/* dtc-lite.py 自动生成以下内容: */
// generated/board_priv_pool.h
typedef union {
    st7789_priv_t       st7789[ST7789_MAX_INSTANCES];
    max98357a_priv_t    max98357a[MAX98357A_MAX_INSTANCES];
    gpio_key_priv_t     gpio_keys[GPIO_KEYS_MAX_INSTANCES];
    ws2812_priv_t       ws2812[WS2812_MAX_INSTANCES];
    /* ... */
} driver_priv_pool_t;

static driver_priv_pool_t s_priv_pool;  /* 全局静态池 */

void* driver_priv_alloc(size_t size) {
    /* 从联合体中分配 (编译期对齐保证) */
    /* 实际实现使用 pool 分配器 */
}
```

---

## 4. 依赖拓扑排序

### 4.1 编译期排序

`dtc-lite.py` 实现拓扑排序:

```python
def topological_sort(tree):
    """Kahn's algorithm — 编译期执行, 非运行时"""

    # 构建邻接表 (directed edge = depends-on)
    graph = {}      # node → list of dependencies
    for node in tree.nodes:
        deps = node.get_prop("depends-on", [])
        graph[node.id] = deps     # dep → node 方向

    # Kahn's algorithm
    in_degree = {}
    for node in graph:
        in_degree[node] = len(graph[node])

    queue = [n for n in graph if in_degree[n] == 0]
    result = []

    while queue:
        n = queue.pop(0)
        result.append(n)
        for m in graph:
            if n in graph[m]:
                in_degree[m] -= 1
                if in_degree[m] == 0:
                    queue.append(m)

    if len(result) != len(graph):
        raise Error("Cycle detected in device dependencies!")

    return result  # 按此顺序 probe
```

### 4.2 排序输出

拓扑排序后, 生成 `board_init.c`:

```c
// generated/board_init.c (简化)

/* init 阶段 1: HAL 基础 (无依赖) */
static int stage1_hal(void) {
    /* spi0, i2s0, uart0 — 无外部依赖 */
    for (int i = 0; i < 3; i++) {  /* 3 = HAL 设备数 */
        int ret = g_probe_table[i].probe_fn(...);
        if (ret < 0) return ret;
    }
    return 0;
}

/* init 阶段 2: 驱动层 (依赖 HAL) */
static int stage2_drivers(void) {
    /* st7789_dev, max98357a_dev — 依赖 spi0, i2s0 */
    for (int i = 3; i < 5; i++) {
        int ret = g_probe_table[i].probe_fn(...);
        if (ret < 0) return ret;
    }
    return 0;
}

/* init 阶段 3: 能力层 (依赖驱动) */
static int stage3_capabilities(void) {
    /* display, audio — 包装驱动为能力接口 */
    ...
}

/* 总入口 */
int board_init_all(void) {
    if (stage1_hal()  < 0) return -1;
    if (stage2_drivers() < 0) return -2;
    if (stage3_capabilities() < 0) return -3;
    return 0;
}
```

### 4.3 排序规则

| 依赖类型 | 示例 | 排序保证 |
|----------|------|----------|
| 显式 `depends-on` | `st7789@0 { depends-on = <&spi0>; }` | spi0 先于 st7789 |
| phandle 属性引用 | `audio { depends-on = <&i2s0>; }` | i2s0 先于 audio |
| chosen 隐式依赖 | `chosen { display = &st7789_dev; }` | st7789 须就绪后才能使用 |
| 无依赖 | `uart0 { ... }` | 最早 init |

### 4.4 循环依赖检测

Python 脚本中检测到循环依赖时报错（非运行时）:

```
ERROR: Circular dependency detected:
  st7789_dev → spi0 → st7789_dev
Fix your board.dts before compilation.
```

---

## 5. Capability 抽象层

### 5.1 设计原则

Capability != Business Service。Capability 回答"能做什么硬件操作"，不回答"业务逻辑是什么"。

```
❌ AudioService          → 业务服务: 播放/暂停/切歌/音量
✅ audio_output_t        → 能力: 输出 PCM 数据到 DAC

❌ CloudService          → 业务服务: WiFi连接/MQTT/数据上报
✅ network_t             → 能力: 发送/接收网络数据包
```

### 5.2 能力类型定义

```c
// capability/display.h — 显示能力

typedef struct display_capability {
    /* 元数据 (由 HAL 层实现填充) */
    int width;
    int height;
    uint8_t bpp;

    /* 操作函数 (纯 C 函数指针) */
    void (*init)(struct display_capability* self);
    void (*fill)(struct display_capability* self,
                 int x, int y, int w, int h, uint32_t color);
    void (*blit)(struct display_capability* self,
                 int x, int y, int w, int h, const uint8_t* data);
    void (*set_brightness)(struct display_capability* self, uint8_t level);
    void (*power)(struct display_capability* self, bool on);
} display_capability_t;
```

```c
// capability/audio_out.h — 音频输出能力

typedef struct audio_output {
    int sample_rate;
    int channels;
    int bits_per_sample;

    int (*init)(struct audio_output* self);
    int (*write)(struct audio_output* self,
                 const int16_t* pcm, uint32_t frames);
    int (*set_volume)(struct audio_output* self, float vol);
    void (*start)(struct audio_output* self);
    void (*stop)(struct audio_output* self);
} audio_output_t;
```

```c
// capability/input.h — 输入能力

typedef enum {
    INPUT_EVENT_PRESS,
    INPUT_EVENT_RELEASE,
    INPUT_EVENT_HOLD,
} input_event_type_t;

typedef struct input_event {
    input_event_type_t type;
    uint32_t           key_id;    /* 映射到 DTS 中的 button label */
    uint64_t           timestamp; /* 硬件定时器 tick */
} input_event_t;

typedef void (*input_callback_t)(const input_event_t* ev, void* ctx);

typedef struct input_collection {
    int key_count;

    int (*init)(struct input_collection* self);
    int (*scan)(struct input_collection* self,
                input_event_t* events, int max_events);
    void (*set_callback)(struct input_collection* self,
                         input_callback_t cb, void* ctx);
} input_collection_t;
```

### 5.3 Capability 实现者

```c
// drivers/display/st7789_driver.c — 作为 display_capability 的实现

#include "capability/display.h"

/* st7789 的显示能力实现 */
static void st7789_display_fill(display_capability_t* self,
                                 int x, int y, int w, int h, uint32_t color) {
    st7789_priv_t* priv = (st7789_priv_t*)
        container_of(self, st7789_priv_t, display_iface);
    /* 实际 SPI 指令序列 */
    st7789_set_window(priv->spi, x, y, x + w - 1, y + h - 1);
    st7789_write_ram(priv->spi, color, w * h);
}

/* probe 时填充能力接口 */
static int st7789_probe(device_t* dev) {
    /* ... 初始化硬件 ... */

    st7789_priv_t* priv = device_get_priv(dev);
    priv->display_iface = (display_capability_t){
        .width  = priv->width,
        .height = priv->height,
        .bpp    = 16,
        .fill   = st7789_display_fill,
        .blit   = st7789_display_blit,
        /* ... */
    };

    /* 注册为显示能力提供者 */
    device_register_capability(dev, CAPABILITY_DISPLAY, &priv->display_iface);
    return 0;
}
```

### 5.4 能力获取 (无 Service Locator)

```c
// app/main.c — 编译期注入, 无运行时查找

#include "generated/board_handles.h"

/* app 接收编译期生成的能力句柄 */
void app_entry(const struct board_capabilities* caps) {
    /* 直接使用能力 — 零查找开销 */
    caps->display->fill(caps->display, 0, 0, 240, 240, 0xFFFF);
    caps->display->set_brightness(caps->display, 128);

    caps->audio_out->set_volume(caps->audio_out, 0.8f);

    if (caps->persistent) {  /* 可选能力: 检查是否为 NULL */
        caps->persistent->read(caps->persistent, 0, buf, 256);
    }
}

/* main.c — 编译期 wiring */
#include "generated/board_handles.h"

void main(void) {
    board_init_all();  /* probe 所有设备 */

    /* 编译期确定的能力分配 (由 chosen 决定) */
    static struct board_capabilities s_caps = {
        .display     = board_get_capability(CHOSEN_DISPLAY),
        .audio_out   = board_get_capability(CHOSEN_AUDIO_OUT),
        .input       = board_get_capability(CAPABILITY_INPUT),
        .indicator   = board_get_capability(CHOSEN_INDICATOR),
        .persistent  = board_get_capability(CAPABILITY_STORAGE),
    };

    app_entry(&s_caps);
}
```

`board_get_capability()` 也是编译期展开的宏:

```c
// generated/board_chosen.h
#define CHOSEN_DISPLAY    DEV_HANDLE(st7789_dev)
#define CHOSEN_AUDIO_OUT  DEV_HANDLE(max98357a_dev)
#define CHOSEN_INDICATOR  DEV_HANDLE(rgb_led_0)
#define CHOSEN_CONSOLE    DEV_HANDLE(uart0)

#define board_get_capability(dev_id) \
    ((void*)device_get_capability(board_device_get(dev_id), \
                                   CAPABILITY_##dev_id))
/* 但更好: 直接写入 app_handles.c 生成 */
```

---

## 6. Runtime 设计

### 6.1 Lifecycle 状态机

```
                    board_init_all()
                        │
       ┌────────────────┤
       ▼                ▼
   HAL_INIT ───▶ DEVICE_PROBE ───▶ CAP_INIT ───▶ SYSTEM_READY
       │                │              │
       ▼                ▼              ▼
    (spi/gpio)     (st7789/ws2812)  (display/audio)

SYSTEM_READY → app_entry(&caps)

app_entry 内部分层 lifecycle:
    INIT → START → RUN ⇄ SUSPEND
      │               │
      └──→ STOP ──────┘
           │
           ▼
        SHUTDOWN
```

### 6.2 Lifecycle Implementation

```c
// runtime/lifecycle.h

typedef enum {
    PHASE_NONE = 0,
    PHASE_HAL_INIT,
    PHASE_DRV_PROBE,
    PHASE_CAP_INIT,
    PHASE_RUNNING,
    PHASE_SUSPENDED,
    PHASE_STOPPED,
    PHASE_FAILED,
} system_phase_t;

/* 系统阶段 transition 合法性 */
static inline bool can_transit(system_phase_t from, system_phase_t to) {
    static const bool valid[8][8] = {
        [PHASE_NONE]       = { [PHASE_HAL_INIT] = true },
        [PHASE_HAL_INIT]   = { [PHASE_DRV_PROBE] = true, [PHASE_FAILED] = true },
        [PHASE_DRV_PROBE]  = { [PHASE_CAP_INIT]  = true, [PHASE_FAILED] = true },
        [PHASE_CAP_INIT]   = { [PHASE_RUNNING]   = true, [PHASE_FAILED] = true },
        [PHASE_RUNNING]    = { [PHASE_SUSPENDED] = true, [PHASE_STOPPED] = true,
                               [PHASE_FAILED] = true },
        [PHASE_SUSPENDED]  = { [PHASE_RUNNING]   = true },
        [PHASE_STOPPED]    = { /* terminal */ },
        [PHASE_FAILED]     = { /* terminal */ },
    };
    return valid[from][to];
}
```

### 6.3 类型安全事件系统 (非 EventBus)

```c
// runtime/events.h — 域事件 (Domain Events)
// 无全局 EventBus, 无 string topic, 无 void* args

/* ── 每个域有独立的事件类型 ── */

// input 域
typedef void (*input_event_callback_t)(const input_event_t* ev, void* ctx);

// media 域  
typedef enum {
    MEDIA_PLAY,
    MEDIA_PAUSE,
    MEDIA_STOP,
    MEDIA_TRACK_CHANGED,
    MEDIA_VOLUME_CHANGED,
} media_event_type_t;

typedef struct {
    media_event_type_t  type;
    union {
        int  track_index;
        float volume;
    };
} media_event_t;

typedef void (*media_event_callback_t)(const media_event_t* ev, void* ctx);

// network 域
typedef enum {
    NET_CONNECTED,
    NET_DISCONNECTED,
    NET_DATA_RECEIVED,
} net_event_type_t;

typedef struct {
    net_event_type_t    type;
    const uint8_t*      data;
    size_t              len;
} net_event_t;

typedef void (*net_event_callback_t)(const net_event_t* ev, void* ctx);
```

```
Domain Event 不经过全局总线:

  input_hw → input_cap.scan() → app_input_handler(ev)
                                    │
                          ┌─────────┼─────────┐
                          ▼         ▼         ▼
                    media_player  ui_nav    serial_debug

  WiFi_hw → net_cap.poll() → app_net_handler(ev)
                                  │
                                  ▼
                              cloud_connector
```

事件路由通过 app 层的静态 dispatch table 实现:

```c
// app/input_handler.c — 静态 dispatch

#include "runtime/events.h"

/* app 层是事件的消费者: 编译期 wiring */
static struct {
    input_event_callback_t handler;
    void*                  ctx;
} s_input_listeners[8];        /* 静态大小, 编译期决定 */

void app_input_dispatch(const input_event_t* ev) {
    for (int i = 0; i < 8 && s_input_listeners[i].handler; i++) {
        s_input_listeners[i].handler(ev, s_input_listeners[i].ctx);
    }
}
```

### 6.4 Runtime 初始化入口

```c
// runtime/init.c — 最终启动入口

#include "runtime/lifecycle.h"
#include "generated/board_init.h"
#include "generated/board_handles.h"

static system_phase_t s_phase = PHASE_NONE;

int runtime_boot(void) {
    /* Phase 1: HAL + Driver probe (按拓扑排序) */
    ASSERT(can_transit(s_phase, PHASE_HAL_INIT));
    if (board_init_all() != 0) {
        s_phase = PHASE_FAILED;
        return -1;
    }
    s_phase = PHASE_DRV_PROBE;

    /* Phase 2: 能力层初始化 */
    ASSERT(can_transit(s_phase, PHASE_CAP_INIT));
    if (capability_init_all() != 0) {
        s_phase = PHASE_FAILED;
        return -1;
    }
    s_phase = PHASE_RUNNING;

    /* Phase 3: app 入口 (编译期注入) */
    app_entry(&g_board_capabilities);   /* g_board_capabilities 由 generated/board_handles.c 定义 */
    return 0;
}
```

---

## 7. HAL 隔离层设计

### 7.1 原则

```
HAL = 纯硬件操作抽象
- 不包含: 状态机、业务逻辑、数据缓冲
- 不暴露: ESP-IDF 类型、第三方 SDK 类型
- 不包含: 协议逻辑 (I2C 读写是 HAL, I2C 传感器协议不是)
```

### 7.2 接口示例

```c
// hal/spi.h — SPI 总线 HAL (纯 C)

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>
#include <stddef.h>

/* ── 不透明类型 ── */
typedef struct hal_spi_bus   hal_spi_bus_t;
typedef struct hal_spi_dev   hal_spi_dev_t;

/* ── 配置结构体 (纯数据, 不依赖任何 SDK) ── */
typedef struct {
    int         mosi_pin;
    int         miso_pin;
    int         sclk_pin;
    int         cs_pin;
    uint32_t    freq_hz;
    uint8_t     mode;           /* 0-3: CPOL/CPHA */
} hal_spi_config_t;

/* ── SPI 总线操作 ── */
int  hal_spi_init(hal_spi_bus_t** bus, const hal_spi_config_t* cfg);
void hal_spi_deinit(hal_spi_bus_t* bus);

/* ── SPI 设备 (片选分开) ── */
int  hal_spi_dev_register(hal_spi_dev_t** dev, hal_spi_bus_t* bus, int cs_pin);
int  hal_spi_transfer(hal_spi_dev_t* dev, const uint8_t* tx, uint8_t* rx, size_t len);

#endif
```

```c
// hal/gpio.h — GPIO HAL

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct hal_gpio      hal_gpio_t;

typedef enum {
    HAL_GPIO_INPUT,
    HAL_GPIO_OUTPUT,
    HAL_GPIO_INPUT_PULLUP,
    HAL_GPIO_INPUT_PULLDOWN,
    HAL_GPIO_OUTPUT_OD,
} hal_gpio_mode_t;

int  hal_gpio_init(hal_gpio_t* gpio, int pin, hal_gpio_mode_t mode);
void hal_gpio_set(hal_gpio_t* gpio, bool value);
bool hal_gpio_get(hal_gpio_t* gpio);
void hal_gpio_deinit(hal_gpio_t* gpio);

#endif
```

### 7.3 平台实现 (ESP-IDF)

```c
// hal/impl/esp32/spi.c — ESP32 SPI HAL 实现

#include "hal/spi.h"
#include "hal/gpio.h"
#include "driver/spi_master.h"      /* ESP-IDF 头文件, 仅在此可见 */

struct hal_spi_bus {
    spi_host_device_t   host;       /* ESP-IDF 类型, 对外部隐藏 */
    spi_device_handle_t handle;
};

int hal_spi_init(hal_spi_bus_t** bus, const hal_spi_config_t* cfg) {
    hal_spi_bus_t* b = calloc(1, sizeof(hal_spi_bus_t));
    if (!b) return -1;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = cfg->mosi_pin,
        .miso_io_num     = cfg->miso_pin,
        .sclk_io_num     = cfg->sclk_pin,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = cfg->freq_hz,
        .mode           = cfg->mode,
        .spics_io_num   = cfg->cs_pin,
        .queue_size     = 1,
    };
    spi_bus_add_device(SPI2_HOST, &dev_cfg, &b->handle);
    b->host = SPI2_HOST;

    *bus = b;
    return 0;
}
```

### 7.4 平台可移植性

```
hal/
├── hal_spi.h              # 公共接口
├── hal_gpio.h
├── port/
│   ├── esp32/             # ESP32 实现
│   │   ├── hal_spi.c
│   │   └── hal_gpio.c
│   ├── stm32/             # STM32 实现
│   │   ├── hal_spi.c
│   │   └── hal_gpio.c
│   └── posix/             # Linux 模拟 (测试用)
│       ├── hal_spi.c
│       └── hal_gpio.c
└── CMakeLists.txt         # target_compile_definitions 选择 port
```

---

## 8. 架构总图

```
┌─────────────────────────────────────────────────────────────────────┐
│  apps /                                                             │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │  ui_app    music_player    serial_debug    cloud_daemon      │  │
│  │  (纯业务组合, 通过 handles 访问能力, 无硬件直接引用)           │  │
│  └───────────────┬──────────────────────────────────────────┬───┘  │
│                  │ handles (编译期注入)                       │      │
├──────────────────▼──────────────────────────────────────────▼───┤
│  runtime /                                                       │
│  ┌──────────────┬──────────────┬──────────────────────────────┐  │
│  │ lifecycle    │ task_manager │ domain events (typed)        │  │
│  │ state machine│ static pool  │ media/net/input (no EventBus)│  │
│  └──────────────┴──────────────┴──────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  capability /                               ← "能做什么"         │
│  ┌──────────┬──────────┬────────┬────────┬────────┐             │
│  │ display  │ audio_out│ input  │network │storage │             │
│  │ fill/blit│ write PCM│ scan   │send/rcv│read/   │             │
│  │ set_bri  │ set_vol  │ event  │connect │write   │             │
│  │ power    │ start/stop│callback│disconn │erase   │             │
│  └────┬─────┴────┬─────┴───┬────┴────┬───┴────┬───┘             │
│       │          │         │         │        │                   │
├───────▼──────────▼─────────▼─────────▼────────▼─────────────────┤
│  drivers /                            ← "操作什么硬件"          │
│  ┌──────────┐ ┌──────────┐ ┌──────┐ ┌────────┐ ┌─────────────┐ │
│  │ st7789   │ │ max98357a│ │gpio  │ │ws2812  │ │ spi_nor     │ │
│  │ +display │ │ +audio   │ │-keys │ │+indicator│ +storage    │ │
│  │ capability│ │ capability│ │+input│ │capability│ capability │ │
│  │ provider │ │ provider │ │cap   │ │provider │ provider     │ │
│  └────┬─────┘ └─────┬────┘ └──┬───┘ └────┬────┘ └──────┬──────┘ │
│       │             │         │          │             │          │
├───────▼─────────────▼─────────▼──────────▼─────────────▼────────┤
│  hal /                                    ← "抽象什么接口"      │
│  ┌──────┬──────┬──────┬──────┬──────┬──────┬─────────────────┐  │
│  │ gpio │ spi  │ i2s  │ pwm  │ uart │ rmt  │ timer          │  │
│  │ i/o  │ xfer │ dma  │ duty │ txrx │ pulse│ delay/         │  │
│  │ ...  │ ...  │ ...  │ ...  │ ...  │ ...  │   timestamp    │  │
│  └──┬───┴──┬───┴──┬───┴──┬───┴──┬───┴──┬───┴────┬──────────┘  │
│     │      │      │      │      │      │        │              │
├─────▼──────▼──────▼──────▼──────▼──────▼────────▼────────────┤
│  board/           ← "编译期生成"                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ generated/                                              │  │
│  │  board_nodes.h     — DEV_ID_xxx 枚举                    │  │
│  │  board_devtable.c  — const device_t g_devices[]        │  │
│  │  board_probe.c     — probe 表 (按拓扑排序)              │  │
│  │  board_handles.h   — struct board_capabilities         │  │
│  │  board_init.c      — board_init_all()                  │  │
│  │  board_aliases.h   — 别名宏                             │  │
│  │  board_chosen.h    — chosen 设备 ID                    │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                 │
│  board.dts — 唯一硬件描述 (板卡选择: 换 dts 即换板)              │
└─────────────────────────────────────────────────────────────────┘
```

### 依赖方向 (单向, 无环)

```
apps → runtime → capability → drivers → hal → board/generated
                                              ↑
                                              board.dts → dtc-lite.py
```

### 数据流

```
启动:
  board.dts → dtc-lite.py → generated/*.c/.h → board_init_all()
                                                    ↓
                                            hal_init → drv_probe → cap_register
                                                                      ↓
                                                              runtime_boot()
                                                                      ↓
                                                              app_entry(&caps)

运行时:
  input GPIO → hal_gpio → gpio_key_driver → input_cap.scan()
       → domain event → app_input_handler → ui_nav / music_player / etc.

  音频:
   app → audio_cap.write(pcm) → max98357a_driver → hal_i2s → ESP-IDF → DAC
```

---

## 9. 如何规避架构坑

### 坑1: Core God Module

**问题**: lifecycle / event / config / log 全塞一个 core 组件，变成万能依赖。

**规避**: 分成独立组件:

```
lifecycle/      — 状态机 enum + transition 检查 (2 个头文件 + 1 个小 .c)
event/          — 域事件类型定义 (仅头文件, 无实现)
config/         — 编译期常量 (`#define` + kconfig, 无运行时)
log/            — 日志宏封装 (仅头文件)
```

每个组件极小、职责单一、零耦合。app 按需引用, 不需要的组件不编译。

### 坑2: EventBus 泛滥

**问题**: 全局 EventBus + 字符串 topic + void* arg → 失控。

**规避**:

```
❌ EventBus::post("music_play", (void*)123)

  类型不安全、运行时字符串比较、不知道谁在消费

✅ media_event_t ev = { .type = MEDIA_PLAY, .track = 123 };
   app_media_handler(&ev);

  类型安全、编译期检查、显式 dispatch
```

规则:
- 不存在全局 EventBus 类型
- 每个域有独立的事件类型和 dispatch 函数
- 事件消费者通过编译期 wiring 注册, 不可运行时动态订阅

### 坑3: Service Locator

**问题**: `FooService::getInstance()` 全局可见, 任何人可调用, 隐藏依赖关系。

**规避**:

```
❌ AudioService::getInstance().play("file.mp3");
❌ CloudService::getInstance().report(data);

✅ app 接收编译期注入的 handles:
   void app_entry(const struct board_capabilities* caps) {
       caps->display->fill(..., color);
   }

   — handles 显式声明 app 需要什么能力
   — 无法在 app 中直接调用未注入的能力
   — 能力实现可替换 (mock 测试)
```

规则:
- 禁止全局 `getInstance()` 或 Service Locator
- 所有跨层调用通过 handles struct 注入
- handles 由编译期代码生成器组装

### 坑4: HAL 泄漏

**问题**: 头文件暴露 ESP-IDF 类型, 导致 drivers 依赖 ESP-IDF。

**规避**:

```
❌ drivers/st7789.h:
       #include "driver/spi_master.h"   ← 泄漏!
       void st7789_init(spi_device_handle_t spi);

✅ hal/spi.h:
       typedef struct hal_spi_bus hal_spi_bus_t;  /* 不透明 */
       int hal_spi_xfer(hal_spi_bus_t* bus, ...);

   drivers/st7789.c:
       #include "hal/spi.h"             ← 仅用 HAL!
       void st7789_init(hal_spi_bus_t* spi);
```

规则:
- HAL 接口头文件只包含 HAL 自身类型, 不包含任何 SDK 头文件
- 不透明类型 (`struct hal_spi_bus` 仅在 .c 文件内定义)
- 平台相关类型仅在 port/*/hal_spi.c 中出现

### 坑5: Driver/Service 混乱

**问题**: 驱动里写业务状态机, service 里直接调用 ESP-IDF。

**规避**:

```
driver/st7789.c:
   职责: 初始化显示控制器、发送 SPI 命令、设置窗口
   不包含: 应用层 UI、字体渲染、主题颜色

capability/display.c:
   职责: 封装显示能力 (fill/blit/brightness)
   不包含: 应用 UI 逻辑、业务状态

app/ui/*.c:
   职责: 菜单、锁屏、设置页
   依赖: display_capability 绘制原语
   不依赖: st7789 驱动
```

### 坑6: JSON Runtime Config

**规避**: MCU 无文件系统, 所有配置在 board.dts 中编译期确定。运行时不需要解析任何 JSON。

```
❌ board.json → cJSON → runtime parser → malloc → device list
✅ board.dts  → dtc-lite.py (编译期) → static C arrays → 零运行时开销
```

### 坑7: Linux 过度复杂化

**规避**:
- 不引入 `#address-cells`, `#size-cells`, `ranges`, `interrupt-parent`
- 不引入 device tree binding schema 验证
- 不引入 runtime module loader
- 不引入 kobject / sysfs
- 不引入 device tree overlays

---

## 10. 可扩展方向

### 10.1 可扩展的

| 方向 | 方式 | 不影响现有架构 |
|------|------|---------------|
| **新板卡** | 新增 `board-xxx.dts` 文件 | ✅ 不碰 C 代码 |
| **新设备** | 在 DTS 中加节点 + 对应 driver | ✅ 不碰其他地方 |
| **新能力** | `capability/` 下新增接口 | ✅ 可选编译 |
| **新平台 (STM32)** | `hal/port/stm32/` 实现 | ✅ HAL 接口不变 |
| **单元测试** | posix port + mock hal | ✅ 无需硬件 |
| **性能分析** | HAL 层加 trace hook | ✅ 可选宏 |
| **电源管理** | lifecycle 加 `SUSPENDED` 状态 | ✅ 已设计 |
| **多任务** | task_manager 加静态线程 | ✅ 不改变依赖 |

### 10.2 明确不可扩展的

| 方向 | 拒绝理由 |
|------|----------|
| Runtime device tree overlay | 违背编译期确定性 |
| 动态驱动加载 (ko-like) | MCU 不需要, 增加复杂度 |
| 全局 EventBus | 违背类型安全 |
| Service Locator | 违背依赖注入 |
| 字符串事件传递 | 违背类型安全 |
| 运行时 DTS 解析 | 违背零运行时开销 |

### 10.3 未来可能的演进

```
v1.0   当前设计: 单板 DTS, 静态 init 表
v1.5   dtc-lite 绑定检查: 在 DTS 编译阶段检查属性是否匹配绑定
       (例: 如果 compatible = "sitronix,st7789v", 检查 width/height/dc-pin 是否存在)
v2.0   电源管理: 在 lifecycle 中集成 PM (suspend/resume 依赖拓扑)
v2.5   安全分区: capability 可加权限检查 (谁可以访问哪些能力)
v3.0   多核 boot: 在 lifecycle 中嵌入 SMP boot 顺序
```

---

## 11. 与当前项目的对照

| 当前项目问题 | 新设计解决方式 |
|-------------|---------------|
| core/ 包含 lifecycle + event_bus + config_store + system_log | 拆分为独立组件, 每件只做一件事 |
| EventBus 用 SystemEvent enum + uintptr_t arg | 域事件, 每个域自有类型和 dispatch |
| Service::getInstance() 全局可访问 | handles 依赖注入, app 只看到 injected caps |
| HAL 暴露 ESP-IDF 类型 | 不透明类型 + 纯 C 接口 |
| JSON runtime 解析 board.dts.json | DTS compile-time 生成 |
| middleware 拆分不干净 | 严格分层: lifecycle/runtime/capability/driver/hal |
| Service 混合业务+硬件 | capability 只做硬件抽象, app 做业务 |
| 驱动未完全自注册 | DRIVER_REGISTER + compile-time 匹配 |
