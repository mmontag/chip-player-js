/* emu2149.h */
#ifndef __EMU2149_H__
#define __EMU2149_H__

#include <stdtype.h>
#include "../snddef.h"
#include "emutypes.h"

#define EMU2149_VOL_DEFAULT 1
#define EMU2149_VOL_YM2149 0
#define EMU2149_VOL_AY_3_8910 1

#define PSG_MASK_CH(x) (1<<(x))

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct __PSG
  {
    DEV_DATA _devData;

    /* Volume Table */
    const uint32_t *voltbl;

    uint8_t reg[0x20];
    //int32_t out;

    uint32_t clk, rate, base_incr, quality;

    uint16_t count[3];
    uint8_t volume[3];
    uint16_t freq[3];
    uint8_t edge[3];
    uint8_t tmask[3];
    uint8_t nmask[3];
    uint32_t mask;
    uint8_t stereo_mask[3];

    uint32_t base_count;

    //uint8_t env_volume;
    uint8_t env_ptr;
    uint8_t env_face;

    uint8_t env_continue;
    uint8_t env_attack;
    uint8_t env_alternate;
    uint8_t env_hold;
    uint8_t env_pause;
    //uint8_t env_reset;

    uint32_t env_freq;
    uint32_t env_count;

    uint32_t noise_seed;
    uint8_t noise_count;
    uint8_t noise_freq;

    /* rate converter */
    uint32_t realstep;
    uint32_t psgtime;
    uint32_t psgstep;
    //int32_t prev, next;
    int32_t sprev[2], snext[2];
    int32_t pan[3][2];

    /* I/O Ctrl */
    uint8_t adr;

    /* output of channels */
    int16_t ch_out[3];

    uint8_t chp_type;
    uint8_t chp_flags;
  } PSG;

  void PSG_set_quality (PSG * psg, uint32_t q);
  void PSG_set_clock(PSG * psg, UINT32 c);
  void PSG_set_rate (PSG * psg, UINT32 r);
  PSG *PSG_new (UINT32 clk, UINT32 rate);
  void PSG_reset (PSG *);
  void PSG_delete (PSG *);
  void PSG_writeReg (PSG *, UINT8 reg, UINT8 val);
  void PSG_writeIO (PSG * psg, UINT8 adr, UINT8 val);
  UINT8 PSG_readReg (PSG * psg, UINT8 reg);
  UINT8 PSG_readIO (PSG * psg, UINT8 adr);
  int16_t PSG_calc (PSG *);
  void PSG_calc_stereo (PSG * psg, UINT32 samples, DEV_SMPL **out);
  void PSG_setFlags (PSG * psg, UINT8 flags);
  void PSG_setVolumeMode (PSG * psg, int type);
  uint32_t PSG_setMask (PSG *, uint32_t mask);
  uint32_t PSG_toggleMask (PSG *, uint32_t mask);
  void PSG_setMuteMask (PSG *, UINT32 mask);
  void PSG_set_pan (PSG * psg, uint8_t ch, int16_t pan);
    
#ifdef __cplusplus
}
#endif

#endif	// __EMU2149_H__
