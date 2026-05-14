# DSP 模块说明

## 1. 目录职责

- `src/dsp.c`：DSP 算法 C 实现。
- `inc/dsp.h`：DSP 对外头文件。
- `py/dsp.ipynb`：算法验证与频域可视化实验脚本。

## 2. 与当前架构关系

- 本目录属于 `algorithm` 层，只放纯算法实现。
- 不直接依赖 `device_hal/service/app`，避免和硬件或协议耦合。
- 实时业务调用在 `app` 层完成，`algorithm` 仅提供计算能力。

## 3. 使用建议

- 先在 `py` 中验证参数与滤波效果，再同步到 `dsp.c`。
- 变更算法参数时，保持 `dsp.h` 接口稳定，减少上层改动。
