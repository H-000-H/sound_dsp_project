#pragma once

#include <cstddef>

#include "lifecycle.hpp"

class ServiceRegistry
{
public:
    static ServiceRegistry& getInstance();

    bool add(Lifecycle* service);
    bool initAll();
    bool startAll();
    void stopAll();
    void suspendAll();
    void resumeAll();

private:
    ServiceRegistry() = default;
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    static constexpr size_t kMaxServices = 12;
    Lifecycle* m_services[kMaxServices] = {};
    size_t m_count = 0;
};
