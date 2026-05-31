#include "tcp_client.hpp"
#if CONFIG_ENABLE_SERVICE_TCP

#include <cerrno>
#include <cstring>

#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <unistd.h>

struct TcpClient::Impl
{
    int socket_fd = -1;
    bool is_connected = false;
};

TcpClient::TcpClient() : m_impl(new Impl) {}

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
    m_impl->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_impl->socket_fd < 0) return false;

    struct timeval snd_timeout = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(m_impl->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &snd_timeout, sizeof(snd_timeout));
    setsockopt(m_impl->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &snd_timeout, sizeof(snd_timeout));

    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, (const char*)ip, &dest.sin_addr);

    int ret = ::connect(m_impl->socket_fd, (struct sockaddr*)&dest, sizeof(dest));
    m_impl->is_connected = (ret == 0);
    return m_impl->is_connected;
}

void TcpClient::disconnect()
{
    if (m_impl->socket_fd >= 0)
    {
        ::close(m_impl->socket_fd);
        m_impl->socket_fd = -1;
    }
    m_impl->is_connected = false;
}

int TcpClient::send(const uint8_t* data, uint16_t len)
{
    if (m_impl->socket_fd < 0) return -1;
    return ::write(m_impl->socket_fd, data, len);
}

int TcpClient::receive(uint8_t* buffer, uint16_t max_len)
{
    if (m_impl->socket_fd < 0) return -1;
    return ::read(m_impl->socket_fd, buffer, max_len);
}
#endif
