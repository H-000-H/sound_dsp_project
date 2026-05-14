# RTC SNTP 编译错误修复计划

## 1. Summary
- 目标：修复 `BSP_RTC_CLOCK.c` 在 ESP-IDF v5.5.2 下的编译错误：`esp_sntp_setservername` 隐式声明。
- 成功标准：`esp-idf/bsp/.../BSP_RTC_CLOCK.c.obj` 可通过编译，不再出现 `-Wimplicit-function-declaration` 报错。
- 范围：仅处理 RTC 网络校时路径 (`bsp_set_rtc_time_net`) 与其头文件依赖关系；不改 UI、不改其他业务逻辑。

## 2. Current State Analysis
- 现状文件：`components/bsp/rtc/src/BSP_RTC_CLOCK.c`
  - 在 `bsp_set_rtc_time_net()` 中调用了：
    - `esp_netif_sntp_init(&handle->sntp_config);`
    - `esp_sntp_setservername(0..2, "...");`
- 现状头文件：`components/bsp/rtc/inc/BSP_RTC_CLOCK.h`
  - 仅包含 `esp_netif_sntp.h`，未直接包含声明 `esp_sntp_setservername` 的头文件。
- SDK 事实（已核对本地 ESP-IDF v5.5.2）：
  - `esp_netif_sntp.h` 定义 `esp_sntp_config_t` 与 `esp_netif_sntp_init`，不声明 `esp_sntp_setservername`。
  - `lwip/include/apps/esp_sntp.h` 中声明了 `esp_sntp_setservername`。
- 结论：当前是“函数声明头缺失”引发的 C 编译错误，不是链接错误。

## 3. Proposed Changes

### 3.1 修复函数声明缺失（最小改动）
- 文件：`components/bsp/rtc/src/BSP_RTC_CLOCK.c`
- 修改内容：
  - 增加 `#include "esp_sntp.h"`（或等价可达路径），确保 `esp_sntp_setservername` 原型可见。
- 原因：
  - C 语言下函数必须先声明；当前编译参数启用 `-Werror=all`，隐式声明会直接失败。

### 3.2 校时配置方式与 IDF v5.5.2 对齐（保持现有行为）
- 文件：`components/bsp/rtc/src/BSP_RTC_CLOCK.c`（必要时）
- 处理策略：
  - 保留 `esp_netif_sntp_init(&handle->sntp_config)`；
  - 保留 `esp_sntp_setservername()` 作为多服务器覆盖设置（与项目既有逻辑一致）。
- 说明：
  - 该策略兼容当前代码结构（`service/time` 里仍通过 `ESP_NETIF_SNTP_DEFAULT_CONFIG(...)` 初始化配置）。
  - 不引入异常机制，保持嵌入式错误处理风格。

### 3.3 可选增强（非必须，若用户同意再做）
- 文件：`components/bsp/rtc/src/BSP_RTC_CLOCK.c`
- 可补充项：
  - 为 `param == NULL` 增加保护分支（与 `bsp_set_rtc_time()` 风格一致）；
  - 检查 `esp_netif_sntp_init` 返回值并记录 `ESP_LOGE`，便于运行期诊断。
- 影响：
  - 不改变对外接口，仅提升健壮性和可观测性。

## 4. Assumptions & Decisions
- 假设：
  - 编译环境为 ESP-IDF v5.5.2（由日志可见）。
  - `bsp` 组件 `REQUIRES lwip esp_netif` 已满足（已在 `components/bsp/CMakeLists.txt` 中确认）。
- 决策：
  - 先执行“最小可行修复”：补齐 `esp_sntp.h` 头文件引用，优先恢复编译。
  - 不做大规模重构（例如改为完全依赖 `esp_sntp_config_t.servers[]`），避免引入行为变更。

## 5. Verification Steps
- 代码层验证：
  - 重新编译目标（至少覆盖 `bsp` 组件）；
  - 确认 `BSP_RTC_CLOCK.c` 不再报 `implicit declaration of function 'esp_sntp_setservername'`。
- 行为层验证（可选）：
  - 启动后观察 SNTP 同步日志；
  - 校验系统时间是否成功更新，且时区设置仍生效（`TZ=CST-8`）。

## 6. Out of Scope
- 不处理与本错误无关的告警/编译问题。
- 不修改 `service/time` API 设计与上层 UI 交互逻辑。
