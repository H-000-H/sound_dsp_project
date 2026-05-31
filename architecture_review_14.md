# 架构整改记录 - 第十四轮

> **⚠️ 归档说明**: 本文件内容已全部合并至 [NOTICE.md](NOTICE.md#第十四轮-应用层终局) 第十四~十六轮。此处作为审计工作底稿归档保留，不再更新。请以 NOTICE.md 为准。

## 概述
本轮审查针对 EventBus、ConfigStore、TaskManager 和 render_engine 四个核心模块，修复了 3 个致命缺陷和 1 个架构瑕疵。

---

## 🚨 致命缺陷一：ConfigStore 子串匹配灾难

### 问题描述
`find_json_value()` 函数使用 `strstr(json, key)` 进行键匹配，导致：
- `getInt("pin", 0)` 在 `{ "backlight_pin": 12, "pin": 5 }` 中会错误匹配到 `"backlight_pin"`
- 返回 `12` 而非正确的 `5`

### 整改方案
构造精确匹配模式 `"key":`：

```c
static const char* find_json_value(const char* key)
{
    const char* json = system_config_json_start;
    if (!json || !key) return NULL;

    size_t key_len = strlen(key);
    if (key_len + 3 > 63) return NULL;

    char search_pat[64];
    search_pat[0] = '"';
    memcpy(search_pat + 1, key, key_len);
    search_pat[key_len + 1] = '"';
    search_pat[key_len + 2] = ':';
    search_pat[key_len + 3] = '\0';

    const char* found = strstr(json, search_pat);
    if (!found) return NULL;

    return found + key_len + 3;
}
```

### 影响文件
- `components/board/src/config_store.c`

---

## 🚨 致命缺陷二：TaskManager 伪静态内存泄漏

### 问题描述
使用 `heap_caps_malloc` + `xTaskCreateStaticPinnedToCore` 创建任务：
- `xTaskCreateStatic` 的语义：用户负责内存管理，FreeRTOS 不会释放
- `vTaskDelete` 时不会自动释放 `stack` 和 `tcb` 内存
- 任务重启或销毁时造成永久性内存泄漏

### 整改方案
回归标准动态 API：

```cpp
TaskHandle_t TaskManager::create(const board_task_config_t& config, TaskEntry entry, void* param)
{
    if (entry == nullptr)
    {
        SYS_LOGE(kTag, "task entry is null: %s", config.name);
        return nullptr;
    }

    TaskHandle_t handle = nullptr;
    const BaseType_t ret = xTaskCreatePinnedToCore(
        entry,
        config.name,
        config.stack_size,
        param,
        config.priority,
        &handle,
        config.core_id);

    if (ret != pdPASS)
    {
        SYS_LOGE(kTag, "failed to create task: %s", config.name);
        return nullptr;
    }

    return handle;
}
```

### 影响