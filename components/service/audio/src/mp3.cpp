#include "mp3.hpp"
void MP3::init()
{
    auto* audio =factory_config::audio::get_audio();
    audio->init();
}

void MP3::play(uint8_t* src_data, uint32_t len,DSP_CLASSFIY classfiy=DSP_CLASSFIY::None_Filtering)
{
    HMP3Decoder dec = MP3InitDecoder();
    uint8_t* read_ptr = src_data;
    int bytes_left = static_cast<int>(len);
    auto* audio = factory_config::audio::get_audio();
    while (bytes_left > 0)
    {
        int sync = MP3FindSyncWord(read_ptr, bytes_left);
        auto* audio = factory_config::audio::get_audio();
        if (sync < 0) break;
        read_ptr += sync;
        bytes_left -= sync;

        int ret = MP3Decode(dec, &read_ptr, &bytes_left, (short*)m_buffer, 0);
        if (ret != 0) continue;

        MP3GetLastFrameInfo(dec, &info);
        size_t size =info.outputSamps*sizeof(short)*2;
        size_t written;
        if(classfiy!=DSP_CLASSFIY::None_Filtering) 
        {
            fifo_write_block(&handle,m_buffer,sizeof(m_buffer));
            audio->play_audio(dsp_process(classfiy,ring_buffer),size,&written,1000);
        }
        else
        {
            audio->play_audio(m_buffer,size,&written,1000);
        }
    }

    MP3FreeDecoder(dec);
}

int16_t* MP3::dsp_process(DSP_CLASSFIY classfiy,int16_t*data)
{
    Fifo_Data_type* p_data=nullptr;

    return (int16_t*)p_data;
}
