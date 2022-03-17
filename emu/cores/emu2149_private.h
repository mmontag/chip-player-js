/* emu2149.h */
#ifndef __EMU2149_H_PRIVATE__
#define __EMU2149_H_PRIVATE__

#include "../../stdtype.h"
#include "../snddef.h"
#include "emutypes.h"

#define EMU2149_VOL_DEFAULT 1
#define EMU2149_VOL_YM2149 0
#define EMU2149_VOL_AY_3_8910 1

#define EPSG_MASK_CH(x) (1<<(x))

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct __EPSG
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
    uint8_t pcm3ch;

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
    uint8_t pcm3ch_detect;
  } EPSG;

  void EPSG_set_quality (EPSG * psg, uint32_t q);
  void EPSG_set_clock(EPSG * psg, UINT32 c);
  void EPSG_set_rate (EPSG * psg, UINT32 r);
  static UINT8 device_start_ay8910_emu(const AY8910_CFG* cfg, DEV_INFO* retDevInf);
  EPSG *EPSG_new (UINT32 clk, UINT32 rate);
  void EPSG_reset (EPSG *);
  void EPSG_delete (EPSG *);
  void EPSG_writeReg (EPSG *, UINT8 reg, UINT8 val);
  void EPSG_writeIO (EPSG * psg, UINT8 adr, UINT8 val);
  UINT8 EPSG_readReg (EPSG * psg, UINT8 reg);
  UINT8 EPSG_readIO (EPSG * psg, UINT8 adr);
  int16_t EPSG_calc (EPSG *);
  void EPSG_calc_stereo (EPSG * psg, UINT32 samples, DEV_SMPL **out);
  void EPSG_setFlags (EPSG * psg, UINT8 flags);
  void EPSG_setVolumeMode (EPSG * psg, int type);
  uint32_t EPSG_setMask (EPSG *, uint32_t mask);
  uint32_t EPSG_toggleMask (EPSG *, uint32_t mask);
  void EPSG_setMuteMask (EPSG *, UINT32 mask);
  void EPSG_setStereoMask (EPSG *psg, UINT32 mask);
  void EPSG_set_pan (EPSG * psg, uint8_t ch, int16_t pan);
  static void ay8910_emu_set_options(void *chip, UINT32 Flags);
  static void ay8910_emu_pan(void* chip, const INT16* PanVals);
    
#ifdef __cplusplus
}
#endif

#endif	// __EMU2149_H_PRIVATE__
