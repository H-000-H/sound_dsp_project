#include "config_store.hpp"

#include <cstdlib>
#include <cstring>

extern "C" const char system_config_json_start[] asm("_binary_system_config_json_start");
extern "C" const char system_config_json_end[] asm("_binary_system_config_json_end");

ConfigStore& ConfigStore::getInstance()
{
    static ConfigStore store;
    return store;
}

bool ConfigStore::init()
{
    m_json = system_config_json_start;
    m_size = static_cast<size_t>(system_config_json_end - system_config_json_start);
    return m_json != nullptr && m_size > 0;
}

const char* ConfigStore::findValue(const char* key) const
{
    if (m_json == nullptr || key == nullptr)
    {
        return nullptr;
    }

    const char* found = strstr(m_json, key);
    if (found == nullptr)
    {
        return nullptr;
    }

    const char* colon = strchr(found, ':');
    if (colon == nullptr)
    {
        return nullptr;
    }

    return colon + 1;
}

bool ConfigStore::getBool(const char* key, bool default_value) const
{
    const char* value = findValue(key);
    if (value == nullptr)
    {
        return default_value;
    }
    if (strstr(value, "true") == value || strstr(value, " true") == value)
    {
        return true;
    }
    if (strstr(value, "false") == value || strstr(value, " false") == value)
    {
        return false;
    }
    return default_value;
}

int ConfigStore::getInt(const char* key, int default_value) const
{
    const char* value = findValue(key);
    return value == nullptr ? default_value : atoi(value);
}

float ConfigStore::getFloat(const char* key, float default_value) const
{
    const char* value = findValue(key);
    return value == nullptr ? default_value : static_cast<float>(atof(value));
}

const char* ConfigStore::getString(const char* key, const char* default_value) const
{
    const char* value = findValue(key);
    if (value == nullptr)
    {
        return default_value;
    }

    const char* first_quote = strchr(value, '"');
    if (first_quote == nullptr)
    {
        return default_value;
    }

    return first_quote + 1;
}
