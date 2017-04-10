//
// Copyright (C) 2013-2016 Alexey Khokholov (Nuke.YKT)
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//  Nuked OPL3 emulator.
//  Thanks:
//      MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
//          Feedback and Rhythm part calculation information.
//      forums.submarine.org.uk(carbon14, opl3):
//          Tremolo and phase generator calculation information.
//      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
//          OPL2 ROMs.
//
// version: 1.7.4
//

#ifndef __NUKEDOPL_H__
#define __NUKEDOPL_H__

#include <stdtype.h>
#include "../snddef.h"

//#define NOPL_ENABLE_WRITEBUF

#define NOPL_WRITEBUF_SIZE   1024
#define NOPL_WRITEBUF_DELAY  2

typedef UINT64          Bit64u;
typedef INT64           Bit64s;
typedef UINT32          Bit32u;
typedef INT32           Bit32s;
typedef UINT16          Bit16u;
typedef INT16           Bit16s;
typedef UINT8           Bit8u;
typedef INT8            Bit8s;

typedef struct _nopl3_slot nopl3_slot;
typedef struct _nopl3_channel nopl3_channel;
typedef struct _nopl3_chip nopl3_chip;

struct _nopl3_slot {
    nopl3_channel *channel;
    nopl3_chip *chip;
    Bit16s out;
    Bit16s fbmod;
    const Bit16s *mod;
    Bit16s prout;
    Bit16s eg_rout;
    Bit16s eg_out;
    Bit8u eg_inc;
    Bit8u eg_gen;
    Bit8u eg_rate;
    Bit8u eg_ksl;
    const Bit8u *trem;
    Bit8u reg_vib;
    Bit8u reg_type;
    Bit8u reg_ksr;
    Bit8u reg_mult;
    Bit8u reg_ksl;
    Bit8u reg_tl;
    Bit8u reg_ar;
    Bit8u reg_dr;
    Bit8u reg_sl;
    Bit8u reg_rr;
    Bit8u reg_wf;
    Bit8u key;
    Bit32u pg_phase;
    Bit32u timer;
};

struct _nopl3_channel {
    nopl3_slot *slots[2];
    nopl3_channel *pair;
    nopl3_chip *chip;
    const Bit16s *out[4];
    Bit8u chtype;
    Bit8u muted;
    Bit16u f_num;
    Bit8u block;
    Bit8u fb;
    Bit8u con;
    Bit8u alg;
    Bit8u ksv;
    Bit16u cha, chb;
};

#ifdef NOPL_ENABLE_WRITEBUF
typedef struct _nopl3_writebuf {
    Bit64u time;
    Bit16u reg;
    Bit8u data;
} nopl3_writebuf;
#endif

struct _nopl3_chip {
    DEV_DATA _devData;  // to alias DEV_DATA struct
    Bit32u clock;
    Bit32u smplRate;
    Bit16u address;

    nopl3_channel channel[18];
    nopl3_slot slot[36];
    Bit16u timer;
    Bit8u isDisabled;
    Bit8u newm;
    Bit8u nts;
    Bit8u rhy;
    Bit8u vibpos;
    Bit8u vibshift;
    Bit8u tremolo;
    Bit8u tremolopos;
    Bit8u tremoloshift;
    Bit32u noise;
    //Bit16s zeromod;
    Bit32s mixbuff[2];
    //OPL3L
    Bit32s rateratio;
    Bit32s samplecnt;
    Bit32s oldsamples[2];
    Bit32s samples[2];

    Bit32u muteMask;
    Bit32s masterVolL;   // master volume left (.12 fixed point)
    Bit32s masterVolR;   // master volume right

#ifdef NOPL_ENABLE_WRITEBUF
    Bit64u writebuf_samplecnt;
    Bit32u writebuf_cur;
    Bit32u writebuf_last;
    Bit64u writebuf_lasttime;
    nopl3_writebuf writebuf[NOPL_WRITEBUF_SIZE];
#endif
};

void NOPL3_Generate(nopl3_chip *chip, Bit32s *buf);
void NOPL3_GenerateResampled(nopl3_chip *chip, Bit32s *buf);
void NOPL3_Reset(nopl3_chip *chip, Bit32u clock, Bit32u samplerate);
void NOPL3_WriteReg(nopl3_chip *chip, Bit16u reg, Bit8u v);
void NOPL3_WriteRegBuffered(nopl3_chip *chip, Bit16u reg, Bit8u v);
void NOPL3_GenerateStream(nopl3_chip *chip, Bit32s *sndptr, Bit32u numsamples);

void nuked_write(void *chip, UINT8 a, UINT8 v);
UINT8 nuked_read(void *chip, UINT8 a);
void nuked_shutdown(void *chip);
void nuked_reset_chip(void *chip);
void nuked_update(void *chip, UINT32 samples, DEV_SMPL **out);
void nuked_set_mutemask(void *chip, UINT32 MuteMask);
void nuked_set_volume(void *chip, INT32 volume);
void nuked_set_vol_lr(void *chip, INT32 volLeft, INT32 volRight);

#endif	// __NUKEDOPL_H__
