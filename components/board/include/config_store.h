#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool config_store_init(void);

bool config_store_get_bool(const char* key, bool default_value);
int  config_store_get_int(const char* key, int default_value);
float config_store_get_float(const char* key, float default_value);
const char* config_store_get_string(const char* key, const char* default_value);

bool config_store_set_bool(const char* key, bool value);
bool config_store_set_int(const char* key, int value);
bool config_store_set_float(const char* key, float value);
bool config_store_set_string(const char* key, const char* value);

bool config_store_commit(void);

bool config_store_factory_reset(void);

int config_store_health(void);

#ifdef __cplusplus
}
#endif

#endif