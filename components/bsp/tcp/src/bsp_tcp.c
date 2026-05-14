#include "bsp_tcp.h"
#if CONFIG_ENABLE_BSP_TCP
#include "lwip/sockets.h"
#include <string.h>
#include <unistd.h>
bool tcp_connect(tcp_handle_t*handle,const uint8_t* ip, uint16_t port)
{
    if(!handle || !ip) return false;
    handle->is_connected=false;
    handle->socket_fd=socket(AF_INET,SOCK_STREAM,0);
    if(handle->socket_fd<0)return false;
    struct sockaddr_in server_addr={0};
    server_addr.sin_family=AF_INET;//指定ipv4的地址族
    server_addr.sin_port=htons(port);
    if(inet_pton(AF_INET,(const char*)ip,&server_addr.sin_addr) != 1)
    {
        close(handle->socket_fd);
        handle->socket_fd = -1;
        return false;
    }
    if(connect(handle->socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))!=0)
    {
        close(handle->socket_fd);
        handle->socket_fd = -1;
        return false;
    }
    handle->is_connected=true;
    return true;
}
void tcp_disconnect(tcp_handle_t*handle)
{
    if(!handle)return ;
    if(handle->socket_fd >= 0)
    {
        close(handle->socket_fd);
        handle->socket_fd = -1;
    }
    handle->is_connected=false;
}
int tcp_send(tcp_handle_t*handle,const uint8_t*data,uint16_t len)
{
    if(!handle||!handle->is_connected)return -1;
    return send(handle->socket_fd,data,len,0);
}
int tcp_receive(tcp_handle_t*handle,uint8_t*buffer,uint16_t max_len)
{
    if(!handle||!handle->is_connected||!buffer)return -1;
    return recv(handle->socket_fd,buffer,max_len,0);
}
#endif
