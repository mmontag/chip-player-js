
#ifndef BYTE

#define BYTE  unsigned char
#define WORD  unsigned short
#define DWORD unsigned long

#endif

#define WAV_CH 2
#define WAV_BPS 2


void write_dword(BYTE *p, DWORD v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
    p[2] = (v>>16) & 0xff;
    p[3] = (v>>24) & 0xff;
}

void write_word(BYTE *p,WORD v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
}

// audio_write_wav_header : ヘッダを出力する
// freq : 再生周波数
// pcm_bytesize : データの長さ
static void audio_write_wav_header(FILE *fp, long freq, long pcm_bytesize)
{
    BYTE hdr[0x80];
    
    if (!fp)
        return;
    
    memcpy(hdr,"RIFF", 4);
    write_dword(hdr + 4, pcm_bytesize + 44);
    memcpy(hdr + 8,"WAVEfmt ", 8);
    write_dword(hdr + 16, 16); // chunk length
    write_word(hdr + 20, 01); // pcm id
    write_word(hdr + 22, WAV_CH); // ch
    write_dword(hdr + 24, freq); // freq
    write_dword(hdr + 28, freq * WAV_CH * WAV_BPS); // bytes per sec
    write_word(hdr + 32, WAV_CH * WAV_BPS); // bytes per frame
    write_word(hdr + 34, WAV_BPS * 8); // bits
    
    memcpy(hdr + 36, "data",4);
    write_dword(hdr + 40, pcm_bytesize); // pcm size
    
    fseek(fp, 0, SEEK_SET);
    fwrite(hdr, 44, 1, fp);
    
    fseek(fp, 0, SEEK_END);
    
}
