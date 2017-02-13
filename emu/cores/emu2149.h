#ifndef __EMU2149_H__
#define __EMU2149_H__

#include <stdtype.h>
#include "../snddef.h"
#include "emutypes.h"

#define EMU2149_API

#define EMU2149_VOL_DEFAULT 1
#define EMU2149_VOL_YM2149 0
#define EMU2149_VOL_AY_3_8910 1

#define PSG_MASK_CH(x) (1<<(x))

typedef struct __PSG
{
  void* chipInf;

  /* Volume Table */
  const e_uint32 *voltbl;

  e_uint8 reg[0x20];
  e_int32 out;
  e_int32 cout[3];

  e_uint32 clk, rate, base_incr, quality;

  e_uint16 count[3];
  e_uint16 freq[3];
  e_uint8 volume[3];
  e_uint8 edge[3];
  e_uint8 tmask[3];
  e_uint8 nmask[3];
  e_uint32 mask;
  e_uint8 stereo_mask[3];

  e_uint32 base_count;

  e_uint8 env_ptr;
  e_uint8 env_face;

  e_uint8 env_continue;
  e_uint8 env_attack;
  e_uint8 env_alternate;
  e_uint8 env_hold;
  e_uint8 env_pause;

  e_uint32 env_freq;
  e_uint32 env_count;

  e_uint32 noise_seed;
  e_uint8 noise_count;
  e_uint8 noise_freq;

  /* rate converter */
  e_uint32 realstep;
  e_uint32 psgtime;
  e_uint32 psgstep;
  e_int32 prev, next;
  e_int32 sprev[2], snext[2];

  /* I/O Ctrl */
  e_uint8 adr;

  e_uint8 chp_type;
  e_uint8 chp_flags;
}
PSG;

EMU2149_API void PSG_set_quality (PSG * psg, e_uint32 q);
EMU2149_API void PSG_set_clock(PSG * psg, UINT32 c);
EMU2149_API void PSG_set_rate (PSG * psg, UINT32 r);
EMU2149_API PSG *PSG_new (UINT32 clk, UINT32 rate);
EMU2149_API void PSG_reset (PSG *);
EMU2149_API void PSG_delete (PSG *);
EMU2149_API void PSG_writeReg (PSG *, UINT8 reg, UINT8 val);
EMU2149_API void PSG_writeIO (PSG * psg, UINT8 adr, UINT8 val);
EMU2149_API UINT8 PSG_readReg (PSG * psg, UINT8 reg);
EMU2149_API UINT8 PSG_readIO (PSG * psg, UINT8 adr);
EMU2149_API void PSG_calc_stereo (PSG * psg, UINT32 samples, DEV_SMPL **out);
EMU2149_API void PSG_setFlags (PSG * psg, UINT8 flags);
EMU2149_API void PSG_setVolumeMode (PSG * psg, UINT8 type);
EMU2149_API void PSG_setMask (PSG *, UINT32 mask);

#endif	// __EMU2149_H__
