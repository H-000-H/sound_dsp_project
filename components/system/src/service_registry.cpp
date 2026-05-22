#include "service_registry.hpp"

ServiceRegistry& ServiceRegistry::getInstance()
{
    static ServiceRegistry registry;
    return registry;
}

bool ServiceRegistry::add(Lifecycle* service)
{
    if (service == nullptr || m_count >= kMaxServices)
    {
        return false;
    }

    m_services[m_count++] = service;
    return true;
}

bool ServiceRegistry::initAll()
{
    for (size_t i = 0; i < m_count; i++)
    {
        if (!m_services[i]->init())
        {
            return false;
        }
    }
    return true;
}

bool ServiceRegistry::startAll()
{
    for (size_t i = 0; i < m_count; i++)
    {
        if (!m_services[i]->start())
        {
            return false;
        }
    }
    return true;
}

void ServiceRegistry::stopAll()
{
    for (size_t i = m_count; i > 0; i--)
    {
        m_services[i - 1]->stop();
    }
}

void ServiceRegistry::suspendAll()
{
    for (size_t i = m_count; i > 0; i--)
    {
        m_services[i - 1]->suspend();
    }
}

void ServiceRegistry::resumeAll()
{
    for (size_t i = 0; i < m_count; i++)
    {
        m_services[i]->resume();
    }
}
