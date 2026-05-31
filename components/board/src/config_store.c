#include "config_store.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_rom_crc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern const char system_config_json_start[] asm("_binary_system_config_json_start");
extern const char system_config_json_end[]   asm("_binary_system_config_json_end");

#define MAX_ENTRIES   32
#define BLOB_MAX      4096

#define NVS_PART      "nvs"
#define NVS_NS_A      "cfg_a"
#define NVS_NS_B      "cfg_b"
#define NVS_NS_META   "cfg_meta"
#define NVS_KEY_FLAG  "flag"
#define NVS_KEY_BLOB  "blob"

#define FLAG_A_VALID  0xA0
#define FLAG_B_VALID  0x0B

typedef enum {
    CS_TYPE_INT    = 0,
    CS_TYPE_FLOAT  = 1,
    CS_TYPE_BOOL   = 2,
    CS_TYPE_STRING = 3,
} cs_type_t;

typedef struct {
    char     key[32];
    cs_type_t type;
    union {
        int   i;
        float f;
        bool  b;
        char  s[64];
    } value;
    bool dirty;
} cs_entry_t;

static cs_entry_t s_entries[MAX_ENTRIES];
static int        s_entry_count;
static int        s_health;
static bool       s_nvs_ready;

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

static cs_entry_t* find_entry(const char* key)
{
    for (int i = 0; i < s_entry_count; i++) {
        if (strcmp(s_entries[i].key, key) == 0)
            return &s_entries[i];
    }
    return NULL;
}

static cs_entry_t* add_entry(const char* key, cs_type_t type)
{
    if (s_entry_count >= MAX_ENTRIES) return NULL;
    cs_entry_t* e = &s_entries[s_entry_count++];
    strncpy(e->key, key, sizeof(e->key) - 1);
    e->key[sizeof(e->key) - 1] = '\0';
    e->type  = type;
    e->dirty = false;
    memset(&e->value, 0, sizeof(e->value));
    return e;
}

static bool load_factory_defaults(void)
{
    const char* json = system_config_json_start;
    if (!json) return false;
    size_t size = (size_t)(system_config_json_end - system_config_json_start);
    if (size == 0) return false;

    s_entry_count = 0;
    const char* p = json;
    while (p < json + size) {
        const char* key_start = strchr(p, '"');
        if (!key_start) break;
        key_start++;
        const char* key_end = strchr(key_start, '"');
        if (!key_end) break;

        size_t key_len = (size_t)(key_end - key_start);
        if (key_len >= 32) { p = key_end + 1; continue; }

        const char* colon = strchr(key_end, ':');
        if (!colon) { p = key_end + 1; continue; }

        const char* val_start = colon + 1;
        while (*val_start == ' ' || *val_start == '\t' || *val_start == '\n' || *val_start == '\r')
            val_start++;

        cs_type_t type;
        if (*val_start == '"') {
            type = CS_TYPE_STRING;
        } else if (*val_start == 't' || *val_start == 'f') {
            type = CS_TYPE_BOOL;
        } else if (strchr(val_start, '.') != NULL) {
            type = CS_TYPE_FLOAT;
        } else {
            type = CS_TYPE_INT;
        }

        char key_buf[32];
        memcpy(key_buf, key_start, key_len);
        key_buf[key_len] = '\0';

        cs_entry_t* e = add_entry(key_buf, type);
        if (e) {
            switch (type) {
            case CS_TYPE_BOOL:
                e->value.b = (strncmp(val_start, "true", 4) == 0);
                break;
            case CS_TYPE_INT:
                e->value.i = atoi(val_start);
                break;
            case CS_TYPE_FLOAT:
                e->value.f = (float)atof(val_start);
                break;
            case CS_TYPE_STRING: {
                const char* q1 = strchr(val_start, '"');
                const char* q2 = q1 ? strchr(q1 + 1, '"') : NULL;
                if (q1 && q2) {
                    size_t slen = (size_t)(q2 - q1 - 1);
                    if (slen >= sizeof(e->value.s)) slen = sizeof(e->value.s) - 1;
                    memcpy(e->value.s, q1 + 1, slen);
                    e->value.s[slen] = '\0';
                }
                break;
            }
            }
        }
        p = val_start + 1;
    }
    return true;
}

static bool blob_serialize(uint8_t* buf, size_t buf_size, size_t* out_len)
{
    size_t pos = 0;
    if (pos + 2 > buf_size) return false;
    buf[pos++] = (uint8_t)(s_entry_count & 0xFF);
    buf[pos++] = (uint8_t)((s_entry_count >> 8) & 0xFF);

    for (int i = 0; i < s_entry_count; i++) {
        cs_entry_t* e = &s_entries[i];
        uint8_t key_len = (uint8_t)strlen(e->key);
        if (pos + 1 + key_len + 1 + 8 > buf_size) return false;

        buf[pos++] = key_len;
        memcpy(buf + pos, e->key, key_len);
        pos += key_len;
        buf[pos++] = (uint8_t)e->type;

        switch (e->type) {
        case CS_TYPE_INT:
            buf[pos++] = (uint8_t)(e->value.i & 0xFF);
            buf[pos++] = (uint8_t)((e->value.i >> 8) & 0xFF);
            buf[pos++] = (uint8_t)((e->value.i >> 16) & 0xFF);
            buf[pos++] = (uint8_t)((e->value.i >> 24) & 0xFF);
            break;
        case CS_TYPE_FLOAT: {
            uint32_t bits;
            memcpy(&bits, &e->value.f, sizeof(bits));
            buf[pos++] = (uint8_t)(bits & 0xFF);
            buf[pos++] = (uint8_t)((bits >> 8) & 0xFF);
            buf[pos++] = (uint8_t)((bits >> 16) & 0xFF);
            buf[pos++] = (uint8_t)((bits >> 24) & 0xFF);
            break;
        }
        case CS_TYPE_BOOL:
            buf[pos++] = e->value.b ? 1 : 0;
            break;
        case CS_TYPE_STRING: {
            uint16_t slen = (uint16_t)strlen(e->value.s);
            buf[pos++] = (uint8_t)(slen & 0xFF);
            buf[pos++] = (uint8_t)((slen >> 8) & 0xFF);
            memcpy(buf + pos, e->value.s, slen);
            pos += slen;
            break;
        }
        }
    }
    *out_len = pos;
    return true;
}

static bool blob_deserialize(const uint8_t* buf, size_t len)
{
    if (len < 2) return false;

    size_t pos = 0;
    uint16_t count = (uint16_t)buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    pos += 2;

    for (uint16_t i = 0; i < count; i++) {
        if (pos >= len) return false;
        uint8_t key_len = buf[pos++];
        if (pos + key_len > len || key_len >= 32) return false;

        char key[32];
        memcpy(key, buf + pos, key_len);
        key[key_len] = '\0';
        pos += key_len;

        if (pos >= len) return false;
        cs_type_t type = (cs_type_t)buf[pos++];

        cs_entry_t* e = add_entry(key, type);
        if (!e) continue;

        switch (type) {
        case CS_TYPE_INT:
            if (pos + 4 > len) return false;
            e->value.i = (int)buf[pos]
                       | ((int)buf[pos + 1] << 8)
                       | ((int)buf[pos + 2] << 16)
                       | ((int)buf[pos + 3] << 24);
            pos += 4;
            break;
        case CS_TYPE_FLOAT: {
            if (pos + 4 > len) return false;
            uint32_t bits = (uint32_t)buf[pos]
                          | ((uint32_t)buf[pos + 1] << 8)
                          | ((uint32_t)buf[pos + 2] << 16)
                          | ((uint32_t)buf[pos + 3] << 24);
            memcpy(&e->value.f, &bits, sizeof(e->value.f));
            pos += 4;
            break;
        }
        case CS_TYPE_BOOL:
            if (pos >= len) return false;
            e->value.b = (buf[pos++] != 0);
            break;
        case CS_TYPE_STRING: {
            if (pos + 2 > len) return false;
            uint16_t slen = (uint16_t)buf[pos] | ((uint16_t)buf[pos + 1] << 8);
            pos += 2;
            if (pos + slen > len) return false;
            if (slen >= sizeof(e->value.s)) slen = (uint16_t)(sizeof(e->value.s) - 1);
            memcpy(e->value.s, buf + pos, slen);
            e->value.s[slen] = '\0';
            pos += slen;
            break;
        }
        }
    }
    return true;
}

static uint8_t read_slot_flag(void)
{
    if (!s_nvs_ready) return 0xFF;

    nvs_handle_t h;
    if (nvs_open(NVS_NS_META, NVS_READONLY, &h) != ESP_OK) return 0xFF;

    uint8_t flag = 0xFF;
    nvs_get_u8(h, NVS_KEY_FLAG, &flag);
    nvs_close(h);
    return flag;
}

static bool write_slot_flag(uint8_t flag)
{
    if (!s_nvs_ready) return false;

    nvs_handle_t h;
    if (nvs_open(NVS_NS_META, NVS_READWRITE, &h) != ESP_OK) return false;

    esp_err_t err = nvs_set_u8(h, NVS_KEY_FLAG, flag);
    if (err != ESP_OK) { nvs_close(h); return false; }
    err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

static bool load_from_nvs(uint8_t slot)
{
    const char* ns_name = (slot == 0) ? NVS_NS_A : NVS_NS_B;
    nvs_handle_t h;
    if (nvs_open(ns_name, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = BLOB_MAX;
    uint8_t buf[BLOB_MAX];
    esp_err_t err = nvs_get_blob(h, NVS_KEY_BLOB, buf, &len);
    nvs_close(h);
    if (err != ESP_OK) return false;
    if (len < 6) return false;

    uint32_t stored_crc;
    memcpy(&stored_crc, buf, sizeof(stored_crc));

    uint32_t calc_crc = esp_rom_crc32_le(0, buf + 4, (uint32_t)(len - 4));
    if (stored_crc != calc_crc) {
        uint8_t alt_slot = (slot == 0) ? 1 : 0;
        uint8_t alt_flag = (alt_slot == 0) ? FLAG_A_VALID : FLAG_B_VALID;
        if (read_slot_flag() != alt_flag) {
            if (load_from_nvs(alt_slot)) {
                write_slot_flag(alt_flag);
                s_health = 1;
                return true;
            }
        }
        s_health = -1;
        load_factory_defaults();
        return true;
    }

    return blob_deserialize(buf + 4, len - 4);
}

static bool save_to_nvs(uint8_t slot)
{
    uint8_t buf[BLOB_MAX];
    size_t out_len = 0;
    if (!blob_serialize(buf + 4, BLOB_MAX - 4, &out_len)) return false;

    uint32_t crc = esp_rom_crc32_le(0, buf + 4, (uint32_t)out_len);
    memcpy(buf, &crc, sizeof(crc));
    out_len += 4;

    const char* ns_name = (slot == 0) ? NVS_NS_A : NVS_NS_B;
    nvs_handle_t h;
    if (nvs_open(ns_name, NVS_READWRITE, &h) != ESP_OK) return false;

    esp_err_t err = nvs_set_blob(h, NVS_KEY_BLOB, buf, out_len);
    if (err != ESP_OK) { nvs_close(h); return false; }
    err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return false;

    uint8_t verify_buf[BLOB_MAX];
    size_t verify_len = BLOB_MAX;
    nvs_handle_t vh;
    if (nvs_open(ns_name, NVS_READONLY, &vh) != ESP_OK) return false;
    esp_err_t verr = nvs_get_blob(vh, NVS_KEY_BLOB, verify_buf, &verify_len);
    nvs_close(vh);
    if (verr != ESP_OK || verify_len != out_len) return false;
    if (memcmp(buf, verify_buf, out_len) != 0) return false;

    return true;
}

bool config_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    s_nvs_ready = (err == ESP_OK);

    if (!load_factory_defaults())
        return false;

    if (s_nvs_ready) {
        uint8_t flag = read_slot_flag();
        if (flag == FLAG_A_VALID) {
            load_from_nvs(0);
        } else if (flag == FLAG_B_VALID) {
            load_from_nvs(1);
        }
    }
    return true;
}

bool config_store_get_bool(const char* key, bool default_value)
{
    cs_entry_t* e = find_entry(key);
    if (e && e->type == CS_TYPE_BOOL) return e->value.b;

    const char* value = find_json_value(key);
    if (!value) return default_value;
    if (strstr(value, "true") == value || strstr(value, " true") == value)
        return true;
    if (strstr(value, "false") == value || strstr(value, " false") == value)
        return false;
    return default_value;
}

int config_store_get_int(const char* key, int default_value)
{
    cs_entry_t* e = find_entry(key);
    if (e && e->type == CS_TYPE_INT) return e->value.i;

    const char* value = find_json_value(key);
    return value ? atoi(value) : default_value;
}

float config_store_get_float(const char* key, float default_value)
{
    cs_entry_t* e = find_entry(key);
    if (e && e->type == CS_TYPE_FLOAT) return e->value.f;

    const char* value = find_json_value(key);
    return value ? (float)atof(value) : default_value;
}

const char* config_store_get_string(const char* key, const char* default_value)
{
    cs_entry_t* e = find_entry(key);
    if (e && e->type == CS_TYPE_STRING) return e->value.s;

    const char* value = find_json_value(key);
    if (!value) return default_value;

    const char* first_quote = strchr(value, '"');
    if (!first_quote) return default_value;
    return first_quote + 1;
}

bool config_store_set_bool(const char* key, bool value)
{
    cs_entry_t* e = find_entry(key);
    if (!e) e = add_entry(key, CS_TYPE_BOOL);
    if (!e) return false;
    e->type = CS_TYPE_BOOL;
    e->value.b = value;
    e->dirty = true;
    return true;
}

bool config_store_set_int(const char* key, int value)
{
    cs_entry_t* e = find_entry(key);
    if (!e) e = add_entry(key, CS_TYPE_INT);
    if (!e) return false;
    e->type = CS_TYPE_INT;
    e->value.i = value;
    e->dirty = true;
    return true;
}

bool config_store_set_float(const char* key, float value)
{
    cs_entry_t* e = find_entry(key);
    if (!e) e = add_entry(key, CS_TYPE_FLOAT);
    if (!e) return false;
    e->type = CS_TYPE_FLOAT;
    e->value.f = value;
    e->dirty = true;
    return true;
}

bool config_store_set_string(const char* key, const char* value)
{
    cs_entry_t* e = find_entry(key);
    if (!e) e = add_entry(key, CS_TYPE_STRING);
    if (!e) return false;
    e->type = CS_TYPE_STRING;
    strncpy(e->value.s, value, sizeof(e->value.s) - 1);
    e->value.s[sizeof(e->value.s) - 1] = '\0';
    e->dirty = true;
    return true;
}

bool config_store_commit(void)
{
    if (!s_nvs_ready) return false;

    uint8_t current = read_slot_flag();
    uint8_t target = (current == FLAG_A_VALID) ? 1 : 0;

    if (!save_to_nvs(target)) return false;

    uint8_t new_flag = (target == 0) ? FLAG_A_VALID : FLAG_B_VALID;
    if (!write_slot_flag(new_flag)) return false;

    for (int i = 0; i < s_entry_count; i++)
        s_entries[i].dirty = false;

    return true;
}

bool config_store_factory_reset(void)
{
    if (!s_nvs_ready) {
        s_entry_count = 0;
        return load_factory_defaults();
    }

    nvs_handle_t h;
    if (nvs_open(NVS_NS_A, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_close(h);
    }
    if (nvs_open(NVS_NS_B, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_close(h);
    }
    if (nvs_open(NVS_NS_META, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_close(h);
    }

    s_entry_count = 0;
    s_health = 0;

    if (!load_factory_defaults()) return false;

    save_to_nvs(0);
    write_slot_flag(FLAG_A_VALID);
    return true;
}

int config_store_health(void)
{
    return s_health;
}