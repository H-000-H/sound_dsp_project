# Factory 使用说明（当前架构）

## 1. 目录职责

- `factory.hpp`：统一对外工厂入口，负责选择并返回 `device_hal` 层设备实例。
- `components/config/config.hpp`：项目统一配置头，负责设备裁剪与时间模式宏配置。

## 2. 当前可用工厂

- `factory_config::screen`：屏幕设备选择（单屏/多屏）。
- `factory_config::led`：LED 设备选择（单灯/多灯）。
- `factory_config::time`：时间设备选择（NTP/非 NTP 模式）。

## 3. 与分层关系

- 本工厂位于 `service`，但当前只做“设备入口编排”，不承载 TCP/MQTT 客户端逻辑。
- 设备实例来自 `device_hal`（如 `st7789.hpp`、`rgb_led.hpp`、`m_time.hpp`）。
- 网络客户端与协议客户端仍在 `service/network` 与 `service/protocol`。

## 4. 裁剪规则

- 在 `components/config/config.hpp` 中通过宏启用唯一方案。
- 单屏与多屏不能同时开启；单灯与多灯不能同时开启。
- 每类设备必须只保留一个有效实现，否则会触发编译期 `#error`。

## 5. 配置建议

- 先确定硬件形态（单屏/多屏、单灯/多灯），再改对应宏。
- 时间模式二选一：`CONFIG_ESP32_TIME_NTP` 或 `CONFIG_ESP32_TIME_WITHOUT_NTP`。
- 宏改动后建议全量重编译，避免旧构建缓存影响判断。
