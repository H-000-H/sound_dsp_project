# 后续开发与修复计划

## 1. 当前状态分析
- **时间模块封装完成**：底层的 `BSP_RTC_CLOCK` 和服务层的 `Time` 类已经完成了重构，修复了宏定义和命名空间的问题，并且解决了 `Time has not been declared` 的编译报错。
- **UI 界面未对接数据**：目前 `start_screen.cpp` 中虽然创建了显示时间的 `lv_label`，但使用的是未初始化的局部变量，且没有加入 LVGL 定时器，导致屏幕会显示乱码且时间不会跳动。
- **残留编译警告**：从之前的编译日志中可以看到存在一些警告，例如 `blue.cpp` 中的 `ISO C++ forbids converting a string constant to 'char*'`，以及 `bsp_blue.c` 中的 API 弃用警告。

## 2. 拟定的修改步骤

### 步骤一：将 Time 服务接入 LVGL 界面
- **目标文件**：`components/app/lvgl/UI/src/start_screen.cpp`
- **操作内容**：
  1. 引入 `#include "m_time.hpp"`。
  2. 增加一个 LVGL 定时器回调函数 `timer_update_time_cb`。
  3. 在回调函数中调用 `Time::get_instance().get_time()` 获取当前系统时间。
  4. 使用 `lv_label_set_text_fmt` 将时间（`%04d-%02d-%02d %02d:%02d:%02d`）动态刷新到屏幕的 label 上。
  5. 在 `start_screen()` 中调用 `lv_timer_create` 启动该定时器（1000ms 周期）。

### 步骤二：修复蓝牙模块的编译警告
- **目标文件 1**：`components/device_hal/blue/src/blue.cpp`
- **操作内容**：
  - 修复第 47 行的 `ISO C++ forbids converting a string constant to 'char*'` 警告。将 `handler.bluebooth_name` 的类型声明修改为 `const char*`，或在赋值时使用 `(char*)"audio box"` 显式转换。
- **目标文件 2**：`components/bsp/blue/src/bsp_blue.c`
- **操作内容**：
  - 修复第 18 行的 `esp_bt_dev_set_device_name is deprecated` 警告。根据 ESP-IDF v5 规范，将其替换为推荐的 `esp_bt_gap_set_device_name`。

## 3. 验证步骤
1. 执行 `idf.py build` 确保没有任何报错（之前的 `m_time.cpp` 错误已修复，新的警告也已被消除）。
2. 烧录到 ESP32 开发板后，观察屏幕是否成功显示 `homelander` 背景图，并且在下方正确显示初始时间（如 `2026-01-01 00:00:00` 或网络同步后的真实时间），且每秒正常跳动。