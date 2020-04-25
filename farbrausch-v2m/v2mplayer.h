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
/*************************************************************************************/
/**                                                                                 **/
/**  Type definitions                                                               **/
/**                                                                                 **/
/*************************************************************************************/

#include <stdint.h>
#include <string.h>
#include "types.h"

#define V2MPLAYER_SYNC_FUNCTIONS

class V2MPlayer
{
public:
    // init
    // call this instead of a constructor
    void Init(uint32_t a_tickspersec = 1000) {
      m_tpc = a_tickspersec;
      /* m_base.valid = 0; */
      memset(&m_base, 0, sizeof(V2MBase));
    }

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
    int Render(float *a_buffer, uint32_t a_len, bool a_add=false);

    // render proxy for C-style callbacks
    //
    // a_this   : void ptr to instance
    // rest as in Render()
    //
    static void RenderProxy(void *a_this, float *a_buffer, uint32_t a_len)
    {
        reinterpret_cast<V2MPlayer*>(a_this)->Render(a_buffer, a_len);
    }

    void SetSpeed(float speed);

    float GetTime();

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
    //           format: 0xBBBBTTNN, where
    //                   BBBB is the bar number (starting at 0)
    //                   TT   is the number of the 32th tick within the current bar
    //                   NN   is the total number of 32th ticks a the current bar has
    //                        (32, normally, may change with different time signatures than 4/4)
    //         ... and so on for every found position
    //
    // NOTE: it is your responsibility to free the array again.
    //
    // returns: number of found song positions
    //
    uint32_t CalcPositions(int32_t **a_dest);

    #endif

#ifdef EMSCRIPTEN
	int* GetVoiceMap();
#endif
    // ------------------------------------------------------------------------------------------------------
    //  no need to look beyond this point.
    // ------------------------------------------------------------------------------------------------------

private:

    // struct defs

    // General info from V2M file
    struct V2MBase {
        bool                   valid;
        const uint8_t          *patchmap;
        uint32_t               base_timediv;
        const uint8_t          *globals;
        uint32_t               timediv;   // Ticks per beat
        uint32_t               timediv2;  // Ticks per beat * 10000
        uint32_t               maxtime;
        const uint8_t          *gptr;     // Global event pointer - pointer to first event
        uint32_t               gd_num;    // Number of global data events

        struct Channel {                  // 16 channels
            uint32_t           note_num;  // Note number
            const uint8_t      *note_ptr; // Note pointer

            uint32_t           pc_num;    // Program change number
            const uint8_t      *pc_ptr;   // Program change pointer

            uint32_t           pb_num;    // Pitch bend number
            const uint8_t      *pb_ptr;   // Pitch bend pointer

            struct CC { // 7 controls
                uint32_t       cc_num;    // Control change number
                const uint8_t  *cc_ptr;   // Control change pointer
            } ctl[7];
        } chan[16];

        const char  *speechdata;
        const char  *speechptrs[256];
        float speed;
    };

    // player state
    struct PlayerState
    {
        enum { OFF, STOPPED, PLAYING, } state;
        uint32_t        time;             // Current time (in ticks?)
        uint32_t        nexttime;         // Time of next event (of any type: g, note, pc, pb, cc)
        const uint8_t   *gptr;            // Global event pointer - updated as song plays
        uint32_t        gnt;              // "nt" generally stands for Next Time
        uint32_t        gnr;              // Global event number
        uint32_t        usecs;

        uint32_t        num;              // Time signature numerator
        uint32_t        den;              // Time signature denominator
        uint32_t        tpq;              // Appears to be unused
        uint32_t        bar;
        uint32_t        beat;
        uint32_t        tick;             // Not sure how this differs from time (appears unused)

        struct Channel
        {
            const uint8_t  *note_ptr;
            uint32_t       note_nr;
            uint32_t       note_nt;
            uint8_t        last_nte;
            uint8_t        last_vel;

            const uint8_t  *pc_ptr;
            uint32_t       pc_nr;
            uint32_t       pc_nt;
            uint8_t        last_pc;

            const uint8_t  *pb_ptr;
            uint32_t       pb_nr;
            uint32_t       pb_nt;
            uint8_t        last_pb0;
            uint8_t        last_pb1;

            struct CC
            {
                const uint8_t  *cc_ptr;
                uint32_t  cc_nt;
                uint32_t  cc_nr;
                uint8_t   last_cc;
            } ctl[7];

        } chan[16];

        uint32_t smpl_cur;
        uint32_t smpl_delta;
        uint32_t smpl_rem;
        uint32_t tdif;
    };

    // \o/
    uint8_t      m_synth[3*1024*1024];   // TODO: keep me uptodate or use "new"

    // member variables
    uint32_t     m_tpc;
    V2MBase      m_base;
    PlayerState  m_state;
    uint32_t     m_samplerate;
    uint8_t      m_midibuf[4096];
    float        m_fadeval;
    float        m_fadedelta;

    // internal methods
    bool InitBase(const void *a_v2m);  // inits base struct from v2m
    void Reset();                      // resets player, inits synth
    void Tick();                       // one midi player tick
};

#endif
