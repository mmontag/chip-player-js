#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-loop-convert"
/*************************************************************************************/
/*************************************************************************************/
/**                                                                                 **/
/**  V2 module player (.v2m)                                                        **/
/**  (c) Tammo 'kb' Hinrichs 2000-2008                                              **/
/**  This file is under the Artistic License 2.0, see LICENSE.txt for details       **/
/**                                                                                 **/
/*************************************************************************************/
/*************************************************************************************/


#include "v2mplayer.h"
#include "libv2.h"
#include <emscripten.h> // TODO: Remove

#define GETDELTA(p, w) ((p)[0] + ((p)[w] << 8) + ((p)[2*w] << 16))
#define UPDATENT(n, v, p, w)  if ((n) < (w)) { (v) = m_state.time + GETDELTA((p), (w)); if ((v) < m_state.nexttime) m_state.nexttime = (v); }
#define UPDATENT2(n, v, p, w) if ((n) < (w) && GETDELTA((p), (w))) { (v) = m_state.time + GETDELTA((p), (w)); }
#define UPDATENT3(n, v, p, w) if ((n) < (w) && (v) < m_state.nexttime) m_state.nexttime = (v);
#define PUTSTAT(s) { uint8_t bla = (s); if (laststat != bla) { laststat = bla; *midibuf ++= (uint8_t)laststat; }};

extern uint32_t readUint(const uint8_t *buf);
extern int readInt(const uint8_t *buf);
extern void writeUint(uint8_t *buf, uint32_t value);

namespace
{
  void UpdateSampleDelta(uint32_t nexttime, uint32_t time, uint32_t usecs, uint32_t td2, uint32_t *smplrem, uint32_t *smpldelta)
  {
    // performs 64bit (nexttime-time)*usecs/td2 and a 32.32bit addition to smpldelta:smplrem
    uint64_t t = ((nexttime - time) * (uint64_t) usecs) / td2; // i.e. usecs=163839879, td2=1280000
    uint32_t r = *smplrem;
    *smplrem   += (t >> 32);        // bits 32-63
    *smpldelta += (t & 0xffffffff); // bits 00-31
    if (*smplrem < r)
    {
      *smpldelta += 1;
    }
  }
}

bool V2MPlayer::InitBase(const void *a_v2m)
{
    const auto *d = (const uint8_t*)a_v2m;

    m_base.speed = 1.0;
    m_base.timediv  = readUint(d);
    m_base.timediv2 = 10000 * m_base.timediv;
    m_base.maxtime  = readUint(d + 4);
    m_base.gd_num   = readUint(d + 8);  // Number of global data events

//  EM_ASM_({ console.log('V2MPlayer::InitBase\n  timediv: %s\n  timediv2: %s\n maxtime: %s, gd_num: %s', $0, $1, $2, $3); }, m_base.timediv, m_base.timediv2, m_base.maxtime, m_base.gd_num);


    d += 12;                            // Advance past timediv, maxtime, and gd_num (each 4 bytes)
    m_base.gptr = d;                    // Store address of first global event
    d += 10 * m_base.gd_num;            // Each global data event is 10 bytes (NOT interleaved)

    // Example global data block for 4 events (40 bytes):
    //
    //    1   2   3   4    <-- gd_num = 4
    // 0
    // 1      ? ? ?        <-- not sure what's here
    // 2
    // 3 [    usecs    ]   <-- usecs is 4 bytes each
    // 4 [    usecs    ]
    // 5 [    usecs    ]
    // 6 [    usecs    ]
    // 7 num num num num   <-- num, den, tpq are 1 byte each
    // 8 den den den den
    // 9 tpq tpq tpq tpq

    for (int ch = 0; ch < 16; ch++)
    {
        V2MBase::Channel &c = m_base.chan[ch];
        c.note_num = readUint(d);
        d += 4;
        if (c.note_num)
        {
            c.note_ptr = d;
            d += 5 * c.note_num;        // Note event: 5 bytes
            c.pc_num = readUint(d);
            d += 4;
            c.pc_ptr = d;
            d += 4 * c.pc_num;          // Program change event: 4 bytes
            c.pb_num = readUint(d);
            d += 4;
            c.pb_ptr = d;
            d += 5 * c.pb_num;          // Pitch bend event: 5 bytes
            for (int cn = 0; cn < 7; cn++)
            {
                V2MBase::Channel::CC &cc = c.ctl[cn];
                cc.cc_num = readUint(d);
                d += 4;
                cc.cc_ptr = d;
                d += 4 * cc.cc_num;     // Control change event: 4 bytes
            }
        }
    }
    int size = readInt(d);
    if (size > 16384 || size < 0)
        return sFALSE;
    d += 4;
    m_base.globals = d;
    d += size;
    size = readInt(d);
    if (size > 1048576 || size < 0)
        return sFALSE;
    d += 4;
    m_base.patchmap = d;
    d += size;

    uint32_t spsize = readUint(d);
    d += 4;
    if (!spsize || spsize >= 8192)
    {
        for (uint32_t i = 0; i < 256; i++)
            m_base.speechptrs[i] = " ";
    } else
    {
        m_base.speechdata = (const char *)d;
        d += spsize;
        const uint32_t *p32 = (const uint32_t*)m_base.speechdata;
        uint32_t n = *(p32++);
        for (uint32_t i = 0; i < n; i++)
        {
            m_base.speechptrs[i] = m_base.speechdata + *(p32++);
        }
    }

  return sTRUE;
}

void V2MPlayer::Reset()
{
    m_state.time = 0;
    m_state.nexttime = (uint32_t)-1; // 4.2 billion

    m_state.gptr = m_base.gptr;
    m_state.gnr  = 0;
    UPDATENT(m_state.gnr, m_state.gnt, m_state.gptr, m_base.gd_num);
    for (int ch = 0; ch < 16; ch++)
    {
        V2MBase::Channel &bc = m_base.chan[ch];
        PlayerState::Channel &sc = m_state.chan[ch];

        if (!bc.note_num)
            continue;
        sc.note_ptr = bc.note_ptr;
        sc.note_nr  = sc.last_nte = sc.last_vel = 0;
        UPDATENT(sc.note_nr,sc.note_nt, sc.note_ptr, bc.note_num);
        sc.pc_ptr = bc.pc_ptr;
        sc.pc_nr  = sc.last_pc=0;
        UPDATENT(sc.pc_nr,sc.pc_nt, sc.pc_ptr, bc.pc_num);
        sc.pb_ptr = bc.pb_ptr;
        sc.pb_nr  = sc.last_pb0 = sc.last_pb1 = 0;
        UPDATENT(sc.pb_nr,sc.pb_nt, sc.pb_ptr, bc.pc_num);
        for (int cn = 0; cn < 7; cn++)
        {
            V2MBase::Channel::CC &bcc = bc.ctl[cn];
            PlayerState::Channel::CC &scc = sc.ctl[cn];
            scc.cc_ptr = bcc.cc_ptr;
            scc.cc_nr  = scc.last_cc=0;
            UPDATENT(scc.cc_nr, scc.cc_nt, scc.cc_ptr, bcc.cc_num);
        }
    }
    m_state.usecs = 5000*m_samplerate; // 500000 microseconds per beat * samplerate / 100
    m_state.num   = 4;
    m_state.den   = 4;
    m_state.tpq   = 8;
    m_state.bar   = 0;
    m_state.beat  = 0;
    m_state.tick  = 0;
    m_state.smpl_rem = 0;

    if (m_samplerate)
    {
        synthInit(m_synth, (void*)m_base.patchmap, m_samplerate);
        synthSetGlobals(m_synth, (void*)m_base.globals);
        synthSetLyrics(m_synth, m_base.speechptrs);
    }
}

void V2MPlayer::Tick()
{
    if (m_state.state != PlayerState::PLAYING)
        return;

    // Track the bar/beat/tick for displaying in a UI, like 4:1:25.
    // tick = nexttime - time
    // beats = ticks / timediv
    // bars = beats / 4 (for example)
    m_state.tick += m_state.nexttime - m_state.time;
    while (m_state.tick >= m_base.timediv)
    {
        m_state.tick -= (uint32_t)(m_base.timediv);
        m_state.beat++;
    }
    uint32_t qpb=(m_state.num*4/m_state.den);
    while (m_state.beat >= qpb)
    {
        m_state.beat -= qpb;
        m_state.bar++;
    }

    // Ok, we are done with using m_state.time
    m_state.time = m_state.nexttime;
    m_state.nexttime = (uint32_t)-1; // 4.2 billion
    uint8_t *midibuf = m_midibuf;
    uint32_t laststat = -1;

    // If we encounter new "global" event
    if (m_state.gnr < m_base.gd_num && m_state.time == m_state.gnt) // neues global-event?
    {
        m_state.usecs = readUint(m_state.gptr + 3*m_base.gd_num + 4*m_state.gnr)*(m_samplerate/100);
        m_state.num = m_state.gptr[7*m_base.gd_num + m_state.gnr];
        m_state.den = m_state.gptr[8*m_base.gd_num + m_state.gnr];
        m_state.tpq = m_state.gptr[9*m_base.gd_num + m_state.gnr];
//        EM_ASM_({ console.log('V2MPlayer::Tick usecs: %s tpq: %s', $0, $1); }, m_state.usecs, m_state.tpq);
        m_state.gnr++;
        UPDATENT2(m_state.gnr, m_state.gnt, m_state.gptr + m_state.gnr, m_base.gd_num);
    }
    UPDATENT3(m_state.gnr, m_state.gnt, m_state.gptr + m_state.gnr, m_base.gd_num);

    for (int ch = 0; ch < 16; ch++)
    {
        V2MBase::Channel &bc = m_base.chan[ch];
        PlayerState::Channel &sc = m_state.chan[ch];
        if (!bc.note_num)
            continue;
        // 1. process pgm change events
        if (sc.pc_nr < bc.pc_num && m_state.time == sc.pc_nt)
        {
            PUTSTAT(0xc0 | ch)
            *midibuf ++= (sc.last_pc += sc.pc_ptr[3*bc.pc_num]);
            sc.pc_nr++;
            sc.pc_ptr++;
            UPDATENT2(sc.pc_nr, sc.pc_nt, sc.pc_ptr, bc.pc_num);
        }
        UPDATENT3(sc.pc_nr, sc.pc_nt, sc.pc_ptr, bc.pc_num);

        // 2. process control changes
        for (int cn = 0; cn < 7; cn++)
        {
                V2MBase::Channel::CC &bcc = bc.ctl[cn];
                PlayerState::Channel::CC &scc = sc.ctl[cn];
                if (scc.cc_nr < bcc.cc_num && m_state.time == scc.cc_nt)
                {
                    PUTSTAT(0xb0 | ch)
                    *midibuf ++= cn+1;
                    *midibuf ++= (scc.last_cc += scc.cc_ptr[3*bcc.cc_num]);
                    scc.cc_nr++;
                    scc.cc_ptr++;
                    UPDATENT2(scc.cc_nr, scc.cc_nt, scc.cc_ptr, bcc.cc_num);
                }
                UPDATENT3(scc.cc_nr, scc.cc_nt, scc.cc_ptr, bcc.cc_num);
        }

        // 3. process pitch bends
        if (sc.pb_nr < bc.pb_num && m_state.time == sc.pb_nt)
        {
            PUTSTAT(0xe0|ch)
            *midibuf ++= (sc.last_pb0 += sc.pb_ptr[3*bc.pc_num]);
            *midibuf ++= (sc.last_pb1 += sc.pb_ptr[4*bc.pc_num]);
            sc.pb_nr++;
            sc.pb_ptr++;
            UPDATENT2(sc.pb_nr,sc.pb_nt,sc.pb_ptr,bc.pb_num);
        }
        UPDATENT3(sc.pb_nr,sc.pb_nt,sc.pb_ptr,bc.pb_num);

        // 4. process notes
        while (sc.note_nr < bc.note_num && m_state.time==sc.note_nt)
        {
            PUTSTAT(0x90 | ch)
            *midibuf ++= (sc.last_nte += sc.note_ptr[3*bc.note_num]);
            *midibuf ++= (sc.last_vel += sc.note_ptr[4*bc.note_num]);
            sc.note_nr++;
            sc.note_ptr++;
            UPDATENT2(sc.note_nr, sc.note_nt, sc.note_ptr, bc.note_num);
        }
        UPDATENT3(sc.note_nr, sc.note_nt, sc.note_ptr, bc.note_num);
    }

    *midibuf ++= 0xfd;

    synthProcessMIDI(m_synth, m_midibuf);

    if (m_state.nexttime == (uint32_t)-1)
        m_state.state = PlayerState::STOPPED;
    if (m_state.time >= m_base.maxtime)
        m_state.state = PlayerState::STOPPED;
}

bool V2MPlayer::Open(const void *a_v2mptr, uint32_t a_samplerate)
{
    if (m_base.valid)
        Close();

    m_samplerate = a_samplerate;

    if (!InitBase(a_v2mptr))
        return sFALSE;

    Reset();
    return m_base.valid = sTRUE;
}

void V2MPlayer::Close()
{
    if (!m_base.valid)
        return;
    if (m_state.state != PlayerState::OFF)
        Stop();
    m_base.valid = 0;
}

void V2MPlayer::Play(uint32_t a_time)
{
    if (!m_base.valid || !m_samplerate)
        return;

    Stop();
    Reset();

    m_base.valid = sFALSE;
    uint32_t destsmpl, cursmpl = 0;
    {
        destsmpl = ((uint64_t)a_time * m_samplerate) / 1000; // m_tpc;
    }

//    EM_ASM_({ console.log('V2MPlayer::Play a_time: %s destsmpl: %s', $0, $1); }, a_time, destsmpl);

    m_state.state = PlayerState::PLAYING;
    m_state.smpl_delta = 0;
    m_state.smpl_rem = 0;
    m_state.smpl_cur = 0;
    while ((cursmpl + m_state.smpl_delta) < destsmpl && m_state.state == PlayerState::PLAYING)
    {
//      EM_ASM_({ console.log('-- cursmpl: %s m_state.time: %s m_state.nexttime: %s', $0, $1, $2); }, cursmpl, m_state.time, m_state.nexttime);

        cursmpl += m_state.smpl_delta;
        Tick();
        if (m_state.state == PlayerState::PLAYING)
        {
            m_state.smpl_delta = 0;
            UpdateSampleDelta(m_state.nexttime,
                              m_state.time,
                              m_state.usecs,
                              m_base.timediv2, // * m_base.speed,
                              &m_state.smpl_rem,
                              &m_state.smpl_delta);
        } else
            m_state.smpl_delta = -1;
    }
    m_state.smpl_cur = cursmpl;
    m_state.smpl_delta = m_state.smpl_delta - (destsmpl - cursmpl);
//    EM_ASM_({ console.log('V2MPlayer::Play m_state.smpl_delta: %s m_state.smpl_cur: %s destsmpl: %s', $0, $1, $2); }, m_state.smpl_delta, m_state.smpl_cur, destsmpl);
    m_fadeval    = 1.0f;
    m_fadedelta  = 0.0f;
    m_base.valid = sTRUE;
}

void V2MPlayer::Stop(uint32_t a_fadetime)
{
    if (!m_base.valid)
        return;

    if (a_fadetime)
    {
        uint32_t ftsmpls = ((uint64_t)a_fadetime * m_samplerate) / m_tpc;
        m_fadedelta = m_fadeval/ftsmpls;
    } else
        m_state.state=PlayerState::OFF;
}
#ifdef EMSCRIPTEN
int* V2MPlayer::GetVoiceMap()
{
	return getVoiceMap(m_synth);
}
#endif
int V2MPlayer::Render(float *a_buffer, uint32_t a_len, bool a_add)
{
    if (!a_buffer)
        return 0;

    int samples_rendered = 0;

    if (m_base.valid && m_state.state == PlayerState::PLAYING) {
//        EM_ASM_({ console.log('V2MPlayer::Render (A) a_len', $0, $1); }, a_len, m_state.state);
        uint32_t todo = a_len;
        while (todo) {
            // how many samples to render?
            // whichever is less of smpl_delta or todo
            int torender = (todo > m_state.smpl_delta) ? m_state.smpl_delta : todo;
            if (torender) {
                synthRender(m_synth, a_buffer, torender, nullptr, a_add);
                a_buffer += 2 * torender;
                todo -= torender;
                m_state.smpl_delta -= torender;
                samples_rendered   += torender;
            }
            // Several consecutive Render calls could complete before entering this branch
            if (!m_state.smpl_delta) {
                // Events are drained. We caught up to new events, and need to process them.
                Tick();
                if (m_state.state == PlayerState::PLAYING)
                    // after Tick, we have a new nexttime, time usecs,
                    UpdateSampleDelta(m_state.nexttime,
                                      m_state.time,
                                      m_state.usecs,
                                      (uint32_t)(m_base.timediv2 * m_base.speed),
                                      &m_state.smpl_rem,
                                      &m_state.smpl_delta);
                else
                    m_state.smpl_delta = -1;
            }
        }
    } else if (m_state.state==PlayerState::OFF || !m_base.valid) {
//      EM_ASM_({ console.log('V2MPlayer::Render (B) a_len', $0, $1); }, a_len, m_state.state);
        if (!a_add) {
            memset(a_buffer, 0, a_len * sizeof(a_buffer[0])*2);
        }
    } else { // PlayerState::STOPPED
//      EM_ASM_({ console.log('V2MPlayer::Render (C) a_len', $0, $1); }, a_len, m_state.state);
        synthRender(m_synth, a_buffer, a_len, nullptr, a_add);
        // Let's try ignoring these samples...!
        // samples_rendered = a_len;
    }

    if (m_fadedelta > 0)
    {
        for (uint32_t i = 0; i < a_len; i++)
        {
            a_buffer[2*i]   *= m_fadeval;
            a_buffer[2*i+1] *= m_fadeval;
            m_fadeval -= m_fadedelta;
            if (m_fadeval < 0)
                m_fadeval = 0;
        }
        if (!m_fadeval) Stop();
    }

    m_state.smpl_cur += (uint32_t)(samples_rendered * m_base.speed);
    return samples_rendered;
}

bool V2MPlayer::NoEnd()
{
    return ((m_base.maxtime * m_base.timediv) > m_state.smpl_cur);
}

uint32_t V2MPlayer::Length()
{
    return ((m_base.maxtime * m_base.timediv) / m_samplerate + 1);
}

bool V2MPlayer::IsPlaying()
{
    return m_base.valid && m_state.state == PlayerState::PLAYING;
}

void V2MPlayer::SetSpeed(float speed) {
  m_base.speed = speed;
}

float V2MPlayer::GetTime() {
  return 1000.0 * m_state.smpl_cur / m_samplerate;
}


#ifdef V2MPLAYER_SYNC_FUNCTIONS

uint32_t V2MPlayer::CalcPositions(int32_t **a_dest)
/////////////////////////////////////////////
{
    if (!a_dest) return 0;
    if (!m_base.valid)
    {
        *a_dest = 0;
        return 0;
    }

    // step 1: ende finden
    int32_t *&dp = *a_dest;
    uint32_t gnr = 0;

    const uint8_t* g_ptr = m_base.gptr;
    uint32_t curbar = 0;
    uint32_t cur32th = 0;
    uint32_t last_ev_time = 0;
    uint32_t pb32 = 0;
    uint32_t usecs = 500000;

    uint32_t posnum = 0;
    uint32_t ttime, time_delta, this32;
    double curtimer = 0;

    while (gnr < m_base.gd_num)
    {
        ttime = last_ev_time + (g_ptr[2*m_base.gd_num] << 16u) + (g_ptr[m_base.gd_num] << 8u) + g_ptr[0];
        time_delta = ttime - last_ev_time;
        this32 = (time_delta*8/m_base.timediv);
        posnum += this32;
        last_ev_time = ttime;
        pb32=g_ptr[7*m_base.gd_num]*32/g_ptr[8*m_base.gd_num];
        gnr++;
        g_ptr++;
    }
    time_delta = m_base.maxtime - last_ev_time;
    this32 = (time_delta * 8 / m_base.timediv);
    posnum += this32 + 1;
    dp = new int32_t[2 * posnum];
    gnr = 0;

    g_ptr = m_base.gptr;
    last_ev_time = 0;
    pb32 = 32;
    uint32_t pn;
    for (pn = 0; pn < posnum; pn++)
    {
        uint32_t curtime = pn*m_base.timediv/8;
        if (gnr < m_base.gd_num)
        {
            ttime = last_ev_time + (g_ptr[2*m_base.gd_num+gnr] << 16u) + (g_ptr[m_base.gd_num + gnr] << 8u) + g_ptr[gnr];
            if (curtime >= ttime)
            {
                pb32=g_ptr[7*m_base.gd_num + gnr]*32/g_ptr[8*m_base.gd_num + gnr];
                usecs=*(uint32_t *)(g_ptr + 3*m_base.gd_num + 4*gnr);
                gnr++;
                last_ev_time=ttime;
            }
        }
        dp[2*pn]     = (uint32_t)curtimer;
        dp[2*pn + 1] = (curbar << 16) | (cur32th << 8) | (pb32);

        cur32th++;
        if (cur32th == pb32)
        {
            cur32th = 0;
            curbar++;
        }
        curtimer += m_tpc * usecs/8000000.0; // 8 million!?
    }
    return pn;
}

#endif

#pragma clang diagnostic pop
