#pragma once

#include <cstddef>

class ConfigStore
{
public:
    static ConfigStore& getInstance();

    bool init();
    bool getBool(const char* key, bool default_value) const;
    int getInt(const char* key, int default_value) const;
    float getFloat(const char* key, float default_value) const;
    const char* getString(const char* key, const char* default_value) const;

private:
    ConfigStore() = default;
    ConfigStore(const ConfigStore&) = delete;
    ConfigStore& operator=(const ConfigStore&) = delete;

    const char* findValue(const char* key) const;

    const char* m_json = nullptr;
    size_t m_size = 0;
};
