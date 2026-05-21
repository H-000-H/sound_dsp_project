/*循环缓冲区源码*/
#include "m_buffer.h"
void fifo_init(FIFO_Type_Def*handle,Fifo_Data_type *buf,uint16_t size)
{
    handle->buf=buf;
    handle->size=size;
    handle->r_ptr=0;
    handle->w_ptr=0;
}

bool fifo_write_data(FIFO_Type_Def*handle,Fifo_Data_type data)
{
    if(fifo_isfull(handle))return false;
    handle->buf[handle->w_ptr]=data;
    handle->w_ptr=(handle->w_ptr+1)%(handle->size);
    return true;
}

uint16_t fifo_write_block(FIFO_Type_Def*handle, const Fifo_Data_type* p_data, uint16_t len)
{
    /*计算剩余空间并且保障了不会覆盖读指针*/
    uint16_t free_len=(handle->size-1)-fifo_get_count(handle);
    if(free_len<len)
    {
        len=free_len;
    }
    if(free_len==0)
    {
        return 0;
    }

    uint16_t space_to_end =handle->size-handle->w_ptr;
    if(space_to_end<len)
    {
        memcpy(&handle->buf[handle->w_ptr],p_data,len*sizeof(Fifo_Data_type));
    }
    else/*因为free_len已经确保了不会掩盖读指针所以可以直接写将数据分两段一端到size一段到r_ptr-1*/
    {   
        memcpy(&handle->buf[handle->w_ptr],p_data,space_to_end*sizeof(Fifo_Data_type));
        memcpy(&handle->buf[0],p_data+space_to_end,(len-space_to_end)*sizeof(Fifo_Data_type));
    }
    handle->w_ptr=(handle->w_ptr+len)%handle->size;
    return len;
}

uint16_t fifo_read_block(FIFO_Type_Def*handle, Fifo_Data_type* p_data, uint16_t len)
{
    memset(p_data,0,sizeof(p_data));
    uint16_t count=fifo_get_count(handle);
    if(len>count)
    {
        len=count;
    }
    if(count==0) return 0;
    /*计算到数组结尾还有多少空间*/
    uint16_t space_to_end=handle->size-handle->r_ptr;
    if(space_to_end>len)
    {
        memcpy(p_data,&handle->buf[handle->r_ptr],len*sizeof(Fifo_Data_type));
    }
    else
    {
        memcpy(p_data,&handle->buf[handle->r_ptr],space_to_end*sizeof(Fifo_Data_type));
        memcpy(p_data+space_to_end,&handle->buf[0],(len-space_to_end)*sizeof(Fifo_Data_type));
    }
    handle->r_ptr=(handle->r_ptr+len)%handle->size;
    return len;
}

bool fifo_read_data(FIFO_Type_Def*handle,Fifo_Data_type*p_data)
{
    if(fifo_isempty(handle)) return false;
    *p_data=handle->buf[handle->r_ptr];
    handle->r_ptr=(handle->r_ptr+1)%handle->size;
    return true;
}

uint16_t fifo_get_count(FIFO_Type_Def*handle)
{
    /*因为是环形缓冲区所以必须加上size不然会出现负数*/
    return (handle->w_ptr + handle->size - handle->r_ptr) % handle->size;
}

bool fifo_isempty(FIFO_Type_Def*handle)
{
    return (handle->r_ptr==handle->w_ptr);
}

bool fifo_isfull(FIFO_Type_Def*handle)
{
    /*约定的环形缓冲区差一个就是满了*/
    return (((handle->w_ptr+1)%handle->size)==handle->r_ptr);
}
