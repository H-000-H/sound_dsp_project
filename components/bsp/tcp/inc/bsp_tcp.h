#ifndef __BSP_TCP_H__
#define __BSP_TCP_H__
#include "config.hpp"
#if CONFIG_ENABLE_BSP_TCP
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct 
{
    int socket_fd;// 套接字文件描述符
    bool is_connected;
}tcp_handle_t;

bool tcp_connect(tcp_handle_t*handle,const uint8_t* ip, uint16_t port);
void tcp_disconnect(tcp_handle_t*handle);
int tcp_send(tcp_handle_t*handle,const uint8_t*data,uint16_t len);
int tcp_receive(tcp_handle_t*handle,uint8_t*buffer,uint16_t max_len);
#ifdef __cplusplus
}
#endif
#endif
#endif
