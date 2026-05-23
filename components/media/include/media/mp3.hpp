#pragma once
#include "mp3dec.h"
#include "EQ.h"

class MP3
{
public:
    void MP3_filter(filter_classfiy type, uint32_t f0, uint32_t Q, int32_t gain_db);
    void MP3_volume(float v);
    void init();
    void play(uint8_t* src_data,uint32_t len);
    static MP3& getinstance(){static MP3 instance; return instance;}
private:
    MP3()=default;
    ~MP3()=default;
    MP3FrameInfo info;
    int16_t m_buffer[8156]={0};
    int32_t q31_buffer[8156]={0};
protected:
};
