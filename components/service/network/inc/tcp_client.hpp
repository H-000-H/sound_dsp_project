#pragma once

#include "config.hpp"
#include <cstdint>

#if CONFIG_ENABLE_SERVICE_TCP
class TcpClient
{
public:
    static TcpClient& get_instance();

    bool connect(const uint8_t* ip, uint16_t port);
    void disconnect();
    int send(const uint8_t* data, uint16_t len);
    int receive(uint8_t* buffer, uint16_t max_len);

private:
    TcpClient();
    ~TcpClient();
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    struct Impl;
    Impl* m_impl;
};
#endif
