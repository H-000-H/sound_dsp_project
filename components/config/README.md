# 配置组件说明

## 目录职责

- `config.hpp`：项目统一编译期配置头文件。
- 集中管理所有模块裁剪、硬件选型、板级引脚、网络参数等宏定义。
- 任何需要读取配置的组件直接包含 `config.hpp`，无需经过 `service/factory` 中转。
- 该组件位于独立层级，禁止 `device_hal` / `service` 反向依赖 `app`。

---

## 宏分类规则

| 前缀 | 用途 | 示例 |
|------|------|------|
| `CONFIG_ENABLE_*` | 模块级裁剪开关，决定某功能是否编译 | `CONFIG_ENABLE_DEVICE_HAL_LCD` |
| `CONFIG_BSP_*` | 板级硬件参数：引脚、时序、极性等 | `CONFIG_BSP_LCD_ST7789_PIN_CLK` |
| `CONFIG_LVGL_*` | LVGL UI 相关参数 | `CONFIG_LVGL_KEY_NEXT_GPIO` |
| `CONFIG_THINGSCLOUD_*` | 云平台接入参数 | `CONFIG_THINGSCLOUD_WIFI_SSID` |
| `CONFIG_LLM_*` | AI API 相关参数 | `CONFIG_LLM_API_BASE_URL` |
| `SOC_*` | 芯片能力宏（来自 ESP-IDF），只读判断 | `SOC_WIFI_SUPPORTED` |

---

## 完整宏定义文档

### 一、device_hal LCD 配置

控制显示子系统的编译与选型。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_ENABLE_DEVICE_HAL_LCD` | `1` | 整体开关：是否编译 LCD 驱动。设为 `0` 则整个 LCD 模块不参与编译。 |
| `CONFIG_ENABLE_LCD_SINGLE_MODE` | `1` | 单屏模式。同一时间只有一块主屏。不可与 `MUX_MODE` 同时为 `1`。 |
| `CONFIG_ENABLE_LCD_MUX_MODE` | `0` | 多屏模式。可同时驱动主屏 + 副屏。不可与 `SINGLE_MODE` 同时为 `1`。 |
| `CONFIG_ENABLE_LCD_MAIN_ST7789_7WIRE` | `1` | 主屏选用 ST7789 7 线 SPI 驱动。SINGLE/MUX 模式下主屏驱动三选一。 |
| `CONFIG_ENABLE_LCD_MAIN_ST7789_8WIRE` | `0` | 主屏选用 ST7789 8 线 SPI 驱动。 |
| `CONFIG_ENABLE_LCD_MAIN_USB_ORANGE_1024X600` | `0` | 主屏选用 USB Orange 1024x600 驱动。 |
| `CONFIG_ENABLE_LCD_SUB_DEVICE` | `0` | 是否启用副屏。仅在 `MUX_MODE=1` 时有意义。 |
| `CONFIG_ENABLE_LCD_SUB_ST7789_7WIRE` | `0` | 副屏选用 ST7789 7 线 SPI 驱动。 |
| `CONFIG_ENABLE_LCD_SUB_ST7789_8WIRE` | `0` | 副屏选用 ST7789 8 线 SPI 驱动。 |
| `CONFIG_ENABLE_LCD_SUB_USB_ORANGE_1024X600` | `0` | 副屏选用 USB Orange 1024x600 驱动。 |

**约束**：
- `SINGLE_MODE` 与 `MUX_MODE` 不能同时为 `1`，同时启用会在编译时报错。
- 单屏模式下，三个主屏驱动宏的和必须精确为 `1`（三选一）。
- 副屏启用时，三个副屏驱动宏的和必须精确为 `1`（三选一）。

### 二、device_hal LED 配置

控制 LED 子系统的编译与选型。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_ENABLE_DEVICE_HAL_LED` | `1` | 整体开关：是否编译 LED 驱动模块。 |
| `CONFIG_ENABLE_LED_SINGLE_MODE` | `1` | 单灯模式。同一时间只有主灯。不可与 `MUX_MODE` 同时为 `1`。 |
| `CONFIG_ENABLE_LED_MUX_MODE` | `0` | 多灯模式。可同时驱动主灯 + 副灯。 |
| `CONFIG_ENABLE_LED_SUB_DEVICE` | `0` | 是否启用副灯。仅在 `MUX_MODE=1` 时有意义。 |
| `CONFIG_ENABLE_LED_MAIN_WS2812` | `1` | 主灯选用 WS2812 RGB LED（通过 RMT 驱动）。 |
| `CONFIG_ENABLE_LED_MAIN_GPIO_SINGLE` | `0` | 主灯选用普通 GPIO 单色 LED。 |
| `CONFIG_ENABLE_LED_SUB_WS2812` | `0` | 副灯选用 WS2812。 |
| `CONFIG_ENABLE_LED_SUB_GPIO_SINGLE` | `0` | 副灯选用普通 GPIO 单色 LED。 |

**约束**：与 LCD 相同，SINGLE/MUX 互斥，选型宏必须且只能有一个为 `1`。

### 三、device_hal Time 配置

控制时间子系统的编译与时源选择。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_ENABLE_DEVICE_HAL_TIME` | `1` | 整体开关：是否编译时间模块（RTC + NTP）。 |
| `CONFIG_ENABLE_TIME_NTP_SYNC` | `1` | 启用 NTP 网络时间同步。不可与 `MANUAL_SET` 同时为 `1`。 |
| `CONFIG_ENABLE_TIME_MANUAL_SET` | `0` | 启用手动设置时间。不可与 `NTP_SYNC` 同时为 `1`。 |

### 四、device_hal 其他模块

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_ENABLE_DEVICE_HAL_BUTTON` | `1` | 按钮驱动开关。 |
| `CONFIG_ENABLE_DEVICE_HAL_BLUE` | `SOC_BT_CLASSIC_SUPPORTED` | 蓝牙驱动开关。自动跟随芯片能力——芯片不支持蓝牙时自动为 `0`。 |
| `CONFIG_ENABLE_DEVICE_HAL_WIFI` | `SOC_WIFI_SUPPORTED` | WiFi 驱动开关。自动跟随芯片能力——芯片不支持 WiFi 时自动为 `0`。 |

### 五、Service 模块配置

控制网络服务层的编译。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_ENABLE_SERVICE_FACTORY` | `1` | 工厂模块开关。关闭后所有依赖 factory 的模块将触发编译错误。 |
| `CONFIG_ENABLE_SERVICE_TCP` | `1` | TCP 客户端服务开关。 |
| `CONFIG_ENABLE_SERVICE_MQTT` | `1` | MQTT 客户端服务开关。 |

### 六、BSP 板级配置

板级支持包开关——决定各外设的 BSP 层是否编译。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| ~~`CONFIG_ENABLE_BSP_CLOCK`~~ | ~~`1`~~ | ~~已删除~~——`esp32_clock.c` 所有函数均为空实现，已移除。 |
| `CONFIG_ENABLE_BSP_GPIO` | `1` | BSP GPIO 初始化模块。 |
| `CONFIG_ENABLE_BSP_RGB_LED` | `1` | BSP RGB LED 初始化模块。 |
| `CONFIG_ENABLE_BSP_SPI` | `1` | BSP SPI 总线初始化模块。 |
| `CONFIG_ENABLE_BSP_I2S` | `1` | BSP I2S 音频总线初始化模块。 |
| `CONFIG_ENABLE_BSP_PWM` | `1` | BSP PWM 初始化模块。 |
| `CONFIG_ENABLE_BSP_LCD_ST7789` | `1` | BSP ST7789 LCD 初始化模块。 |
| `CONFIG_ENABLE_BSP_WIFI` | `SOC_WIFI_SUPPORTED` | BSP WiFi 初始化模块。自动跟随芯片能力。 |
| `CONFIG_ENABLE_BSP_TCP` | `1` | BSP TCP 初始化模块。 |
| `CONFIG_ENABLE_BSP_MQTT` | `1` | BSP MQTT 初始化模块。 |
| `CONFIG_ENABLE_BSP_BLUE` | `SOC_BT_CLASSIC_SUPPORTED` | BSP 蓝牙初始化模块。自动跟随芯片能力。 |
| `CONFIG_ENABLE_BSP_RTC` | `1` | BSP RTC 初始化模块。 |
| ~~`CONFIG_ENABLE_BSP_MAX98357A`~~ | ~~`1`~~ | ~~已删除~~——BSP 层为空文件，MAX98357A 驱动由 Device HAL 直接实现。 |

### 七、BSP 硬件参数

ST7789 LCD 引脚定义（7 线 SPI 模式）：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_BSP_LCD_ST7789_PIN_CLK` | `4` | SPI 时钟引脚 (SCLK) |
| `CONFIG_BSP_LCD_ST7789_PIN_MOSI` | `5` | SPI 数据引脚 (MOSI) |
| `CONFIG_BSP_LCD_ST7789_PIN_DC` | `6` | 数据/命令选择引脚 (DC) |
| `CONFIG_BSP_LCD_ST7789_PIN_RST` | `7` | 复位引脚 (RST) |
| `CONFIG_BSP_LCD_ST7789_PIN_BLK` | `15` | 背光控制引脚 (BLK) |
| `CONFIG_BSP_LCD_ST7789_BACKLIGHT_ACTIVE_HIGH` | `1` | 背光极性：`1`=高电平亮，`0`=低电平亮 |

RGB LED 参数：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_BSP_RGB_LED_DEFAULT_BRIGHTNESS` | `28` | WS2812 默认亮度（0-255） |
| `CONFIG_BSP_RGB_LED_RMT_RESOLUTION_HZ` | `10_000_000` | RMT 驱动分辨率 (10 MHz) |

蓝牙扫描默认参数：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_BSP_BLUE_DEFAULT_INQ_LEN` | `10` | 蓝牙 Inquiry 扫描时长（秒） |
| `CONFIG_BSP_BLUE_DEFAULT_NUM_RSPS` | `0` | 蓝牙 Inquiry 最大返回设备数（`0`=不限） |

RTC / NTP 参数：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_BSP_RTC_TIMEZONE` | `"CST-8"` | 时区（中国标准时间，UTC+8） |
| `CONFIG_BSP_RTC_NTP_SERVER_0` | `"ntp.aliyun.com"` | NTP 服务器（首选） |
| `CONFIG_BSP_RTC_NTP_SERVER_1` | `"time.pool.aliyun.com"` | NTP 服务器（备用 1） |
| `CONFIG_BSP_RTC_NTP_SERVER_2` | `"cn.pool.ntp.org"` | NTP 服务器（备用 2） |

### 八、LVGL 物理按键 GPIO 配置

定义 LVGL UI 导航所用的物理按键 GPIO 编号。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_LVGL_KEY_NEXT_GPIO` | `16` | "下一个" 按键 GPIO |
| `CONFIG_LVGL_KEY_PREV_GPIO` | `17` | "上一个" 按键 GPIO |
| `CONFIG_LVGL_KEY_ENTER_GPIO` | `3` | "确认" 按键 GPIO |
| `CONFIG_LVGL_KEY_ESC_GPIO` | `46` | "返回" 按键 GPIO |
| `CONFIG_LVGL_KEY_DEBOUNCE_MS` | `50` | 按键消抖时间（毫秒） |

### 九、ThingsCloud 云平台配置

WiFi 接入参数：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_THINGSCLOUD_WIFI_SSID` | `"plus"` | WiFi 热点名称 |
| `CONFIG_THINGSCLOUD_WIFI_PASSWORD` | `"22334455hh"` | WiFi 密码 |
| `CONFIG_THINGSCLOUD_WIFI_TIMEOUT_MS` | `10000` | WiFi 连接超时（毫秒） |

MQTT 接入参数：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_THINGSCLOUD_MQTT_URI` | `"mqtt://gz-4-mqtt.iot-api.com:1883"` | MQTT Broker URI |
| `CONFIG_THINGSCLOUD_MQTT_CLIENT_ID` | `"wpzbaryy"` | MQTT 客户端 ID |
| `CONFIG_THINGSCLOUD_MQTT_USERNAME` | `"33hsg6n6aygssr8j"` | MQTT 用户名 |
| `CONFIG_THINGSCLOUD_MQTT_PASSWORD` | `"Ae0toMoRrM"` | MQTT 密码 |

### 十、LLM API 配置

AI 大模型 API 接入参数（兼容 OpenAI 格式，例：DeepSeek）。

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CONFIG_LLM_API_ENABLE` | `0` | 整体开关：是否编译 LLM API 客户端。默认关闭。 |
| `CONFIG_LLM_API_BASE_URL` | `"https://api.deepseek.com/v1"` | API 端点地址。可替换为任意 OpenAI 兼容 API。 |
| `CONFIG_LLM_API_KEY` | `""` | API 密钥。生产环境建议通过 KConfig / 环境变量注入，勿硬编码提交。 |
| `CONFIG_LLM_API_MODEL` | `"deepseek-chat"` | 模型名称。 |
| `CONFIG_LLM_API_TIMEOUT_MS` | `30000` | HTTP 请求超时（毫秒）。 |

---

## 使用原则

1. **模块裁剪**：`CONFIG_ENABLE_*` 控制功能是否编译。关闭后对应模块完全排除，节省 Flash/RAM。
2. **选型互斥**：有互斥关系的宏（如 `SINGLE_MODE` vs `MUX_MODE`，`NTP_SYNC` vs `MANUAL_SET`）通过 `#if/#error` 在编译期拦截非法组合。
3. **三选一约束**：LCD 和 LED 的主屏/副屏驱动宏要求精确等于 `1`，防止未选或多选。
4. **芯片能力跟随**：WiFi 和蓝牙的宏默认值自动跟随 `SOC_WIFI_SUPPORTED` / `SOC_BT_CLASSIC_SUPPORTED`，避免在不支持的芯片上误编译。
5. **修改方式**：所有宏可通过 KConfig (`idf.py menuconfig`) 覆盖，也可在 `config.hpp` 中直接修改默认值。推荐使用 KConfig 保持可追溯性。
