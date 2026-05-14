#ifndef __BUFF_H__
#define __BUFF_H__
#ifdef __cplusplus
extern "C"
{
#endif
#include<stdint.h>
#include<stdbool.h>
#include<string.h>
/*
----------------------------------------------------------------------------------------------------------------------------------------------------------
----------------------------------------------------------------------------------------------------------------------------------------------------------
        以下是环形缓冲区代码
*/
typedef int16_t Fifo_Data_type;
typedef struct
{
    Fifo_Data_type* buf;
    volatile uint16_t w_ptr;//必须加防止编译器优化
    volatile uint16_t r_ptr;
    uint16_t size;
}FIFO_Type_Def;

/**
 * @brief 环形缓冲区初始化
 * @param buf为静态缓冲区的地址
 */
void fifo_init(FIFO_Type_Def*handle,Fifo_Data_type *buf,uint16_t size);

/**
 * @brief 环形缓冲区写函数
 * @details false为发送失败或者写满
 */
bool fifo_write_data(FIFO_Type_Def*handle,Fifo_Data_type data);

/**
 * @环形缓冲区读函数
 * @param p_data为数据所在位置的地址
 * @details false为缓冲区空或者读取失败
 */
bool fifo_read_data(FIFO_Type_Def*handle,Fifo_Data_type*p_data);

/**
 * @brief 环形缓冲区批量写函数
 * @param p_data 要写入的数据指针
 * @param len 准备写入的长度
 * @return 实际成功写入的长度
 */
uint16_t fifo_write_block(FIFO_Type_Def*handle, const Fifo_Data_type* p_data, uint16_t len);

/**
 * @brief 环形缓冲区批量读函数
 * @param p_data 读取数据的存放指针
 * @param len 准备读取的最大长度
 * @return 实际成功读取的长度
 */
uint16_t fifo_read_block(FIFO_Type_Def*handle, Fifo_Data_type* p_data, uint16_t len);
/*判断是否为满*/
bool fifo_isfull(FIFO_Type_Def*handle);

/*判断是否空*/
bool fifo_isempty(FIFO_Type_Def*handle);

/*判断数量*/
uint16_t fifo_get_count(FIFO_Type_Def*handle);

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif
#endif
