#include "tcp_client.hpp"
#if CONFIG_ENABLE_SERVICE_TCP

extern "C"
{
#include "bsp_tcp.h"
}

struct TcpClient::Impl
{
    tcp_handle_t handle = {};
};

TcpClient::TcpClient() : m_impl(new Impl)
{
}

TcpClient::~TcpClient()
{
    delete m_impl;
}

TcpClient& TcpClient::get_instance()
{
    static TcpClient instance;
    return instance;
}

bool TcpClient::connect(const uint8_t* ip, uint16_t port)
{
    return tcp_connect(&m_impl->handle, ip, port);
}

void TcpClient::disconnect()
{
    tcp_disconnect(&m_impl->handle);
}

int TcpClient::send(const uint8_t* data, uint16_t len)
{
    return tcp_send(&m_impl->handle, data, len);
}

int TcpClient::receive(uint8_t* buffer, uint16_t max_len)
{
    return tcp_receive(&m_impl->handle, buffer, max_len);
}
#endif
