#ifndef __EMU2413_H__
#define __EMU2413_H__

#include <stdtype.h>
#include "../snddef.h"
#include "emutypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum OPLL_TONE_ENUM {OPLL_2413_TONE=0, OPLL_VRC7_TONE=1, OPLL_281B_TONE=2} ;

/* voice data */
typedef struct __OPLL_PATCH {
  uint32_t TL,FB,EG,ML,AR,DR,SL,RR,KR,KL,AM,PM,WF ;
} OPLL_PATCH ;

/* slot */
typedef struct __OPLL_SLOT {

  const OPLL_PATCH *patch;

  int32_t type ;          /* 0 : modulator 1 : carrier */

  /* OUTPUT */
  int32_t feedback ;
  int32_t output[2] ;   /* Output value of slot */

  /* for Phase Generator (PG) */
  uint16_t *sintbl ;    /* Wavetable */
  uint32_t phase ;      /* Phase */
  uint32_t dphase ;     /* Phase increment amount */
  uint32_t pgout ;      /* output */

  /* for Envelope Generator (EG) */
  int32_t fnum ;          /* F-Number */
  int32_t block ;         /* Block */
  int32_t volume ;        /* Current volume */
  int32_t sustine ;       /* Sustine 1 = ON, 0 = OFF */
  uint32_t tll ;	      /* Total Level + Key scale level*/
  uint32_t rks ;        /* Key scale offset (Rks) */
  int32_t eg_mode ;       /* Current state */
  uint32_t eg_phase ;   /* Phase */
  uint32_t eg_dphase ;  /* Phase increment amount */
  uint32_t egout ;      /* output */

} OPLL_SLOT ;

/* Mask */
#define OPLL_MASK_CH(x) (1<<(x))
#define OPLL_MASK_HH (1<<(9))
#define OPLL_MASK_CYM (1<<(10))
#define OPLL_MASK_TOM (1<<(11))
#define OPLL_MASK_SD (1<<(12))
#define OPLL_MASK_BD (1<<(13))
#define OPLL_MASK_RHYTHM ( OPLL_MASK_HH | OPLL_MASK_CYM | OPLL_MASK_TOM | OPLL_MASK_SD | OPLL_MASK_BD )

/* opll */
typedef struct __OPLL {

  DEV_DATA _devData;

  /* Input clock */
  uint32_t clk;
  /* Sampling rate */
  uint32_t rate;

  uint8_t vrc7_mode;
  uint8_t adr ;
  int32_t out ;

  uint32_t realstep ;
  uint32_t oplltime ;
  uint32_t opllstep ;
  //int32_t prev, next ;
  int32_t sprev[2],snext[2];
  float pan[15][2];

  /* Register */
  uint8_t reg[0x40] ;
  int32_t slot_on_flag[18] ;

  /* Pitch Modulator */
  uint32_t pm_phase ;
  int32_t lfo_pm ;

  /* Amp Modulator */
  int32_t am_phase ;
  int32_t lfo_am ;

  uint8_t quality;
  uint8_t native;   /* running at native sample rate */

  /* Noise Generator */
  uint32_t noise_seed ;

  /* Channel Data */
  int32_t patch_number[9];
  int32_t key_status[9] ;

  /* Slot */
  OPLL_SLOT slot[18] ;

  /* Voice Data */
  OPLL_PATCH patch[19*2] ;
  int32_t patch_update[2] ; /* flag for check patch update */

  /* Phase delta for LFO */
  uint32_t pm_dphase;
  uint32_t am_dphase;

  /* Phase incr table for Attack */
  uint32_t dphaseARTable[16][16];
  /* Phase incr table for Decay and Release */
  uint32_t dphaseDRTable[16][16];

  /* Phase incr table for PG */
  uint32_t dphaseTable[512][8][16];

  uint32_t mask ;

  /* Output of each channels / 0-8:TONE, 9:BD 10:HH 11:SD, 12:TOM, 13:CYM, 14:Reserved for DAC */
  int16_t ch_out[15];

} OPLL ;

/* Create Object */
OPLL *OPLL_new(uint32_t clk, uint32_t rate) ;
void OPLL_delete(OPLL *) ;

/* Setup */
void OPLL_reset(OPLL *) ;
void OPLL_reset_patch(OPLL *, int32_t) ;
void OPLL_set_rate(OPLL *opll, uint32_t r) ;
void OPLL_set_quality(OPLL *opll, uint32_t q) ;
void OPLL_set_pan(OPLL *, uint32_t ch, int32_t pan);

/* Port/Register access */
void OPLL_writeIO(OPLL *, UINT8 reg, UINT8 val) ;
void OPLL_writeReg(OPLL *, UINT8 reg, UINT8 val) ;

/* Synthsize */
int16_t OPLL_calc(OPLL *) ;
void OPLL_calc_stereo (OPLL * opll, UINT32 samples, DEV_SMPL **out) ;

/* Misc */
void OPLL_setPatch(OPLL *, const uint8_t *dump) ;
void OPLL_copyPatch(OPLL *, int32_t, OPLL_PATCH *) ;
void OPLL_forceRefresh(OPLL *) ;
/* Utility */
void OPLL_dump2patch(const uint8_t *dump, OPLL_PATCH *patch) ;
void OPLL_patch2dump(const OPLL_PATCH *patch, uint8_t *dump) ;
void OPLL_getDefaultPatch(int32_t type, int32_t num, OPLL_PATCH *) ;

/* Channel Mask */
uint32_t OPLL_setMask(OPLL *, uint32_t mask) ;
uint32_t OPLL_toggleMask(OPLL *, uint32_t mask) ;
void OPLL_SetMuteMask(OPLL* opll, UINT32 MuteMask);
void OPLL_SetChipMode(OPLL* opll, UINT8 Mode);

#ifdef __cplusplus
}
#endif

#endif	// __EMU2413_H__
