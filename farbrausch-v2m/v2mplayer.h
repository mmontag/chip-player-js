/*************************************************************************************/
/*************************************************************************************/
/**                                                                                 **/
/**  V2 module player (.v2m)                                                        **/
/**  (c) Tammo 'kb' Hinrichs 2000-2008                                              **/
/**  This file is under the Artistic License 2.0, see LICENSE.txt for details       **/
/**                                                                                 **/
/*************************************************************************************/
/*************************************************************************************/

#pragma once
#ifndef V2MPLAYER_H_
#define V2MPLAYER_H_

#define PROJECTNAME "V2MPlayer"
#define PROJECTVERSION "0.20180825"
#define PROJECTURL "github.com/jgilje"


/*************************************************************************************/
/**                                                                                 **/
/**  Type definitions                                                               **/
/**                                                                                 **/
/*************************************************************************************/

#include <stdint.h>
#include <string.h>
#include "types.h"

class V2MPlayer
{
public:
    // init
    // call this instead of a constructor
    void Init(uint32_t a_tickspersec=1000) { m_tpc=a_tickspersec; /* m_base.valid=0; */ memset(&m_base, 0, sizeof(V2MBase)); }

    // opens a v2m file for playing
    //
    // a_v2mptr            : ptr to v2m data
    //                                NOTE: the memory block has to remain valid
    //                                as long as the player is opened!
    // a_samplerate : samplerate at which the synth is operating
    //                if this is zero, output will be disabled and
    //                only the timing query functions will work

    // returns  : flag if succeeded
    //
    bool Open(const void *a_v2mptr, uint32_t a_samplerate=44100);

    // closes player
    //
    void Close();

    // starts playing
    //
    // a_time   : time offset from song start in msecs
    //
    void Play(uint32_t a_time=0);

    // stops playing
    //
    // a_fadetime : optional fade out time in msecs
    //
    void Stop(uint32_t a_fadetime=0);

    // render call (to be used from sound thread et al)
    // renders samples (or silence if not playing) into buffer
    //
    // a_buffer : ptr to stereo float sample buffer (0dB=1.0f)
    // a_len    : number of samples to render
    //
    // returns  : flag if playing
    //
    void Render(float *a_buffer, uint32_t a_len, bool a_add=0);

    // render proxy for C-style callbacks
    //
    // a_this   : void ptr to instance
    // rest as in Render()
    //
    static void RenderProxy(void *a_this, float *a_buffer, uint32_t a_len)
    {
        reinterpret_cast<V2MPlayer*>(a_this)->Render(a_buffer, a_len);
    }

  bool NoEnd();
  uint32_t Length();

  // returns if song is currently playing
    bool IsPlaying();

    #ifdef V2MPLAYER_SYNC_FUNCTIONS
    // Retrieves an array of timer<->song position
    //
    // a_dest: pointer to a variable which will receive the address of an array of long
    //         values structured as following:
    //         first  long: time in ms
    //         second long: song position (see above for a description)
    //                                            format: 0xBBBBTTNN, where
    //                                                            BBBB is the bar number (starting at 0)
    //                                                            TT   is the number of the 32th tick within the current bar
    //                                                            NN   is the total number of 32th ticks a the current bar has
    //                                                             (32, normally, may change with different time signatures than 4/4)
    //         ... and so on for every found position
    //
    // NOTE: it is your responsibility to free the array again.
    //
    // returns: number of found song positions
    //
    uint32_t CalcPositions(int32_t **a_dest);

    #endif


    // ------------------------------------------------------------------------------------------------------
    //  no need to look beyond this point.
    // ------------------------------------------------------------------------------------------------------

private:

    // struct defs

    // General info from V2M file
    struct V2MBase
    {
        bool valid;
        const uint8_t   *patchmap;
        const uint8_t   *globals;
        uint32_t    timediv;
        uint32_t    timediv2;
        uint32_t    maxtime;
        const uint8_t   *gptr;
        uint32_t  gdnum;
        struct Channel
        {
            uint32_t    notenum;
            const uint8_t        *noteptr;
            uint32_t    pcnum;
            const uint8_t        *pcptr;
            uint32_t    pbnum;
            const uint8_t        *pbptr;
            struct CC {
                uint32_t    ccnum;
                const uint8_t        *ccptr;
            } ctl[7];
        } chan[16];
        const char  *speechdata;
        const char  *speechptrs[256];
    };

    // player state
    struct PlayerState
    {
        enum { OFF, STOPPED, PLAYING, } state;
        uint32_t    time;
        uint32_t    nexttime;
        const uint8_t        *gptr;
        uint32_t    gnt;
        uint32_t    gnr;
        uint32_t  usecs;
        uint32_t  num;
        uint32_t  den;
        uint32_t    tpq;
        uint32_t  bar;
        uint32_t  beat;
        uint32_t  tick;
        struct Channel
        {
            const uint8_t  *noteptr;
            uint32_t       notenr;
            uint32_t       notent;
            uint8_t        lastnte;
            uint8_t        lastvel;
            const uint8_t  *pcptr;
            uint32_t       pcnr;
            uint32_t       pcnt;
            uint8_t        lastpc;
            const uint8_t  *pbptr;
            uint32_t       pbnr;
            uint32_t       pbnt;
            uint8_t        lastpb0;
            uint8_t        lastpb1;
            struct CC
            {
                const uint8_t  *ccptr;
                uint32_t  ccnt;
                uint32_t  ccnr;
                uint8_t   lastcc;
            } ctl[7];
        } chan[16];
        uint32_t cursmpl;
        uint32_t smpldelta;
        uint32_t smplrem;
        uint32_t tdif;
    };

    // \o/
    uint8_t      m_synth[3*1024*1024];   // TODO: keep me uptodate or use "new"

    // member variables
    uint32_t     m_tpc;
    V2MBase      m_base;
    PlayerState  m_state;
    uint32_t     m_samplerate;
    int32_t      m_timeoffset;
    uint8_t      m_midibuf[4096];
    float        m_fadeval;
    float        m_fadedelta;

    // internal methods
    bool InitBase(const void *a_v2m);  // inits base struct from v2m
    void Reset();                      // resets player, inits synth
    void Tick();                       // one midi player tick
};

#endif
