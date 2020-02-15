//
// Copyright (C) 2013-2018 Alexey Khokholov (Nuke.YKT)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
//  Nuked OPL3 emulator.
//  Thanks:
//      MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
//          Feedback and Rhythm part calculation information.
//      forums.submarine.org.uk(carbon14, opl3):
//          Tremolo and phase generator calculation information.
//      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
//          OPL2 ROMs.
//      siliconpr0n.org(John McMaster, digshadow):
//          YMF262 and VRC VII decaps and die shots.
//
// version: 1.8
//

// Nuked OPL3 internal structures/functions
#ifndef __NUKEDOPL_INT_H__
#define __NUKEDOPL_INT_H__

#include "../../stdtype.h"
#include "../snddef.h"

//#define NOPL_ENABLE_WRITEBUF

#define OPL_WRITEBUF_SIZE   1024
#define OPL_WRITEBUF_DELAY  2

typedef UINT64          Bit64u;
typedef INT64           Bit64s;
typedef UINT32          Bit32u;
typedef INT32           Bit32s;
typedef UINT16          Bit16u;
typedef INT16           Bit16s;
typedef UINT8           Bit8u;
typedef INT8            Bit8s;

typedef struct _opl3_slot opl3_slot;
typedef struct _opl3_channel opl3_channel;
typedef struct _opl3_chip opl3_chip;

struct _opl3_slot {
    opl3_channel *channel;
    opl3_chip *chip;
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
    Bit32u pg_reset;
    Bit32u pg_phase;
    Bit16u pg_phase_out;
    Bit8u slot_num;
};

struct _opl3_channel {
    opl3_slot *slots[2];
    opl3_channel *pair;
    opl3_chip *chip;
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
    Bit8u ch_num;
};

typedef struct _opl3_writebuf {
    Bit64u time;
    Bit16u reg;
    Bit8u data;
} opl3_writebuf;

struct _opl3_chip {
    DEV_DATA _devData;  // to alias DEV_DATA struct
    Bit32u clock;
    Bit32u smplRate;
    Bit16u address;

    opl3_channel channel[18];
    opl3_slot slot[36];
    Bit16u timer;
    Bit64u eg_timer;
    Bit8u eg_timerrem;
    Bit8u eg_state;
    Bit8u eg_add;
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
    Bit8u rm_hh_bit2;
    Bit8u rm_hh_bit3;
    Bit8u rm_hh_bit7;
    Bit8u rm_hh_bit8;
    Bit8u rm_tc_bit3;
    Bit8u rm_tc_bit5;
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
    opl3_writebuf writebuf[OPL_WRITEBUF_SIZE];
#endif
};

void NOPL3_Generate(opl3_chip *chip, Bit32s *buf);
void NOPL3_GenerateResampled(opl3_chip *chip, Bit32s *buf);
void NOPL3_Reset(opl3_chip *chip, Bit32u clock, Bit32u samplerate);
void NOPL3_WriteReg(opl3_chip *chip, Bit16u reg, Bit8u v);
void NOPL3_WriteRegBuffered(opl3_chip *chip, Bit16u reg, Bit8u v);
void NOPL3_GenerateStream(opl3_chip *chip, Bit32s *sndptr, Bit32u numsamples);

#endif	// __NUKEDOPL_INT_H__
