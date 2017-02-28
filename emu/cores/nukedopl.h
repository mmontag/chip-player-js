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
    Bit16s *mod;
    Bit16s prout;
    Bit16s eg_rout;
    Bit16s eg_out;
    Bit8u eg_inc;
    Bit8u eg_gen;
    Bit8u eg_rate;
    Bit8u eg_ksl;
    Bit8u *trem;
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

struct _opl3_channel {
    opl3_slot *slots[2];
    opl3_channel *pair;
    opl3_chip *chip;
    Bit16s *out[4];
    Bit8u chtype;
    Bit16u f_num;
    Bit8u block;
    Bit8u fb;
    Bit8u con;
    Bit8u alg;
    Bit8u ksv;
    Bit16u cha, chb;
};

typedef struct _opl3_writebuf {
    Bit64u time;
    Bit16u reg;
    Bit8u data;
} opl3_writebuf;

struct _opl3_chip {
    void* chipInf;   // to alias DEV_DATA struct
    Bit32u clock;
    Bit32u smplRate;
    Bit16u address;

    opl3_channel channel[18];
    opl3_slot slot[36];
    Bit16u timer;
    Bit8u newm;
    Bit8u nts;
    Bit8u rhy;
    Bit8u vibpos;
    Bit8u vibshift;
    Bit8u tremolo;
    Bit8u tremolopos;
    Bit8u tremoloshift;
    Bit32u noise;
    Bit16s zeromod;
    Bit32s mixbuff[2];
    //OPL3L
    Bit32s rateratio;
    Bit32s samplecnt;
    Bit16s oldsamples[2];
    Bit16s samples[2];

    Bit64u writebuf_samplecnt;
    Bit32u writebuf_cur;
    Bit32u writebuf_last;
    Bit64u writebuf_lasttime;
    opl3_writebuf writebuf[OPL_WRITEBUF_SIZE];
};

void OPL3_Generate(opl3_chip *chip, Bit16s *buf);
void OPL3_GenerateResampled(opl3_chip *chip, Bit16s *buf);
void OPL3_Reset(opl3_chip *chip, Bit32u clock, Bit32u samplerate);
void OPL3_WriteReg(opl3_chip *chip, Bit16u reg, Bit8u v);
void OPL3_WriteRegBuffered(opl3_chip *chip, Bit16u reg, Bit8u v);
void OPL3_GenerateStream(opl3_chip *chip, Bit16s *sndptr, Bit32u numsamples);

void nuked_write(void *chip, UINT8 a, UINT8 v);
void nuked_shutdown(void *chip);
void nuked_reset_chip(void *chip);
void nuked_update(void *chip, UINT32 samples, DEV_SMPL **out);

#endif	// __NUKEDOPL_H__
