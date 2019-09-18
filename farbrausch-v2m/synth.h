#pragma once

extern "C"
{
    extern unsigned int synthGetSize();

    extern void synthInit(void *pthis, const void *patchmap, int samplerate=44100);
    extern void synthRender(void *pthis, void *buf, int smp, void *buf2=0, int add=0);
    extern void synthProcessMIDI(void *pthis, const void *ptr);
    extern void synthSetGlobals(void *pthis, const void *ptr);
//  extern void synthSetSampler(void *pthis, const void *bankinfo, const void *samples);
    extern void synthGetPoly(void *pthis, void *dest);
    extern void synthGetPgm(void *pthis, void *dest);
//  extern void synthGetLD(void *pthis, float *l, float *r);

    // only if VUMETER define is set in synth source

    // vu output values are floats, 1.0 == 0dB
    // you might want to clip or logarithmize the values for yourself
    extern void synthSetVUMode(void *pthis, int mode); // 0: peak, 1: rms
    extern void synthGetChannelVU(void *pthis, int ch, float *l, float *r); // ch: 0..15
    extern void synthGetMainVU(void *pthis, float *l, float *r);

    extern long synthGetFrameSize(void *pthis);

#ifdef RONAN
    extern void synthSetLyrics(void *pthis, const char **ptr);
#endif

}
