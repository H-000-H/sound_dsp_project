#include "device.h"

#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* kTag = "board_dts";

/* ── 内部设备节点结构 (device_t 在头文件中是不透明指针, 此处完整定义) ── */
#define MAX_PROPS     16
#define PROP_KEY_LEN  32
#define PROP_VAL_LEN  64

typedef struct {
    char key[PROP_KEY_LEN];
    char value[PROP_VAL_LEN];   /* 统一存为字符串, accessor 按需转型 */
} device_prop_t;

typedef struct device_node {
    char          name[32];
    char          compatible[48];
    int           parent_idx;        /* -1 = 无 parent */
    device_prop_t props[MAX_PROPS];
    int           prop_count;
    device_status_t status;
    void*         priv_data;
} device_node_t;

/* ── 静态设备表 ── */
static device_node_t s_devices[MAX_DEVICES];
static int           s_device_count = 0;

/* ── 内嵌 JSON 文件符号 (由 EMBED_TXTFILES 生成) ── */
extern const uint8_t _binary_board_dts_json_start[];
extern const uint8_t _binary_board_dts_json_end[];

/* ======================================================================== */
/*  JSON 解析                                                               */
/* ======================================================================== */
static int parse_properties(cJSON* json_props, device_node_t* node)
{
    if (!json_props || !cJSON_IsObject(json_props)) return -1;

    int count = 0;
    cJSON* child = NULL;
    cJSON_ArrayForEach(child, json_props)
    {
        if (count >= MAX_PROPS) break;
        if (!child->string) continue;

        strncpy(node->props[count].key, child->string, PROP_KEY_LEN - 1);
        node->props[count].key[PROP_KEY_LEN - 1] = '\0';

        /* 统一存为字符串 */
        if (cJSON_IsString(child))
        {
            strncpy(node->props[count].value, child->valuestring, PROP_VAL_LEN - 1);
        }
        else if (cJSON_IsNumber(child))
        {
            snprintf(node->props[count].value, PROP_VAL_LEN, "%d", child->valueint);
        }
        else if (cJSON_IsBool(child))
        {
            snprintf(node->props[count].value, PROP_VAL_LEN, "%d", cJSON_IsTrue(child) ? 1 : 0);
        }
        else
        {
            continue;   /* 跳过数组/对象等复杂类型 */
        }
        node->props[count].value[PROP_VAL_LEN - 1] = '\0';
        count++;
    }
    return count;
}

static int find_device_by_name(const char* name)
{
    for (int i = 0; i < s_device_count; i++)
    {
        if (strcmp(s_devices[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int parse_device_node(cJSON* json_dev)
{
    if (s_device_count >= MAX_DEVICES)
    {
        ESP_LOGE(kTag, "too many devices, max %d", MAX_DEVICES);
        return -1;
    }

    device_node_t* node = &s_devices[s_device_count];
    memset(node, 0, sizeof(device_node_t));
    node->parent_idx = -1;
    node->status = DEVICE_STATUS_DISABLED;

    /* name */
    cJSON* name = cJSON_GetObjectItem(json_dev, "name");
    if (!cJSON_IsString(name))
    {
        ESP_LOGW(kTag, "device missing 'name', skip");
        return -1;
    }
    strncpy(node->name, name->valuestring, sizeof(node->name) - 1);

    /* compatible */
    cJSON* compat = cJSON_GetObjectItem(json_dev, "compatible");
    if (cJSON_IsString(compat))
    {
        strncpy(node->compatible, compat->valuestring, sizeof(node->compatible) - 1);
    }

    /* depends_on → parent_idx */
    cJSON* deps = cJSON_GetObjectItem(json_dev, "depends_on");
    if (cJSON_IsString(deps))
    {
        int idx = find_device_by_name(deps->valuestring);
        if (idx >= 0)
            node->parent_idx = idx;
        else
            ESP_LOGW(kTag, "device '%s' depends_on '%s' not found (forward ref?)",
                     node->name, deps->valuestring);
    }

    /* properties */
    cJSON* props = cJSON_GetObjectItem(json_dev, "properties");
    if (cJSON_IsObject(props))
    {
        parse_properties(props, node);
    }

    /* status — 兼容 Linux DTS style: "okay"/"disabled" */
    cJSON* status = cJSON_GetObjectItem(json_dev, "status");
    if (cJSON_IsString(status))
    {
        if (strcmp(status->valuestring, "okay") == 0)
            node->status = DEVICE_STATUS_READY;
    }
    else
    {
        node->status = DEVICE_STATUS_READY;  /* 默认 ok */
    }

    s_device_count++;
    return 0;
}

int device_tree_init(void)
{
    /* 计算 JSON 长度 */
    size_t json_len = (size_t)(_binary_board_dts_json_end - _binary_board_dts_json_start);
    if (json_len == 0)
    {
        ESP_LOGE(kTag, "empty board.dts.json");
        return -1;
    }

    /* cJSON 解析 */
    cJSON* root = cJSON_ParseWithLength((const char*)_binary_board_dts_json_start, json_len);
    if (!root)
    {
        ESP_LOGE(kTag, "JSON parse error");
        return -1;
    }

    ESP_LOGI(kTag, "board model: %s",
             cJSON_GetObjectItem(root, "model") ?
             cJSON_GetObjectItem(root, "model")->valuestring : "unknown");

    /* 解析 devices 数组 */
    cJSON* devices = cJSON_GetObjectItem(root, "devices");
    if (cJSON_IsArray(devices))
    {
        int count = cJSON_GetArraySize(devices);
        for (int i = 0; i < count; i++)
        {
            cJSON* item = cJSON_GetArrayItem(devices, i);
            if (item) parse_device_node(item);
        }
    }

    cJSON_Delete(root);

    ESP_LOGI(kTag, "loaded %d devices from board.dts.json", s_device_count);
    return 0;
}

/* ======================================================================== */
/*  device_t API 实现                                                        */
/* ======================================================================== */
device_t* device_find(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < s_device_count; i++)
    {
        if (strcmp(s_devices[i].name, name) == 0)
            return &s_devices[i];
    }
    return NULL;
}

device_t* device_find_by_compatible(const char* compatible)
{
    if (!compatible) return NULL;
    for (int i = 0; i < s_device_count; i++)
    {
        if (strcmp(s_devices[i].compatible, compatible) == 0)
            return &s_devices[i];
    }
    return NULL;
}

device_t* device_get_parent(const device_t* dev)
{
    if (!dev || dev->parent_idx < 0) return NULL;
    return &s_devices[dev->parent_idx];
}

int device_get_prop_int(const device_t* dev, const char* key, int* val)
{
    if (!dev || !key || !val) return -1;
    for (int i = 0; i < dev->prop_count; i++)
    {
        if (strcmp(dev->props[i].key, key) == 0)
        {
            *val = atoi(dev->props[i].value);
            return 0;
        }
    }
    return -1;
}

int device_get_prop_str(const device_t* dev, const char* key, const char** val)
{
    if (!dev || !key || !val) return -1;
    for (int i = 0; i < dev->prop_count; i++)
    {
        if (strcmp(dev->props[i].key, key) == 0)
        {
            *val = dev->props[i].value;
            return 0;
        }
    }
    return -1;
}

int device_get_prop_bool(const device_t* dev, const char* key, int* val)
{
    return device_get_prop_int(dev, key, val);
    /* caller 用 if (*val) 判断, 0=假 非0=真 */
}

const char* device_get_name(const device_t* dev)
{
    return dev ? dev->name : NULL;
}

const char* device_get_compatible(const device_t* dev)
{
    return dev ? dev->compatible : NULL;
}

device_status_t device_get_status(const device_t* dev)
{
    return dev ? dev->status : DEVICE_STATUS_DISABLED;
}

int device_set_status(device_t* dev, device_status_t status)
{
    if (!dev) return -1;
    dev->status = status;
    return 0;
}

int device_set_priv(device_t* dev, void* priv)
{
    if (!dev) return -1;
    dev->priv_data = priv;
    return 0;
}

void* device_get_priv(const device_t* dev)
{
    return dev ? dev->priv_data : NULL;
}

device_t* device_get_first(void)
{
    return s_device_count > 0 ? &s_devices[0] : NULL;
}

device_t* device_get_next(const device_t* prev)
{
    if (!prev) return NULL;
    ptrdiff_t idx = prev - s_devices;
    if (idx < 0 || idx >= s_device_count - 1) return NULL;
    return &s_devices[idx + 1];
}

int device_get_count(void)
{
    return s_device_count;
}
