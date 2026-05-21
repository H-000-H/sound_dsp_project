#pragma once
#include "factory.hpp"
#include "mp3dec.h"
#include "m_buffer.h"

enum DSP_CLASSFIY
{
    None_Filtering=0,
    Environment_sound_strength,
    People_sound_strength,
    sef_set
};

typedef void (*mp3_dsp_cb)(int16_t* data, size_t len);

class MP3
{
public:
    void init();
    void play(uint8_t* src_data,uint32_t len,DSP_CLASSFIY classfiy=DSP_CLASSFIY::None_Filtering);
    static MP3& getinstance(){static MP3 instance; return instance;}
    int16_t* dsp_process(DSP_CLASSFIY classfiy,int16_t*data);
private:
    MP3()=default;
    ~MP3()=default;
    volatile bool is_dsp;
    MP3FrameInfo info;
    int16_t m_buffer[8156]={0};/*pcm的输出缓冲区*/
    Fifo_Data_type* ring_buffer=nullptr;
    FIFO_Type_Def handle={ring_buffer,0,0,8156};/*正常不使用这个如果启动dsp就启用这个*/
protected:
};