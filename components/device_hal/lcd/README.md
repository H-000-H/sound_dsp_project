# HAL LCD 驱动要点（ST7789 240x240）

当前项目默认使用 ST7789（240x240）SPI 屏，按无独立 CS 的 7 线方案配置。

## 1. 当前文件位置

- BSP 头文件：`components/bsp/lcd/inc/st7789.h`
- BSP 源文件：`components/bsp/lcd/src/st7789.c`
- HAL LCD 封装：`components/device_hal/lcd/inc/st7789.hpp`
- HAL LCD 实现：`components/device_hal/lcd/src/st7789.cpp`
- HAL SPI 封装：`components/device_hal/spi/inc/spi_bus.hpp`

## 2. 默认硬件配置

- 分辨率：`240x240`
- 颜色格式：`RGB565`
- SPI 模式：`mode 3`
- 无独立 `CS`，驱动按无 `CS` 逻辑发送

## 3. 调试重点

- 花屏/颜色异常：优先检查 SPI 模式与 `COLMOD/MADCTL` 配置。
- 背光亮但无图像：检查 `DC/RST/BLK` 管脚定义与电平。
- 初始化异常：检查 SPI 初始化、DMA 缓冲申请和 `draw_bitmap` 调用链。

## 4. 与架构关系

- `device_hal/lcd` 只负责屏幕设备抽象与刷新调用。
- 具体图形渲染由 `app/lvgl` 完成。
- 协议与联网逻辑不在 LCD 层处理。
