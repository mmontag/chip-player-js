#ifndef __EMU2413_H_PRIVATE__
#define __EMU2413_H_PRIVATE__

#include <stdtype.h>
#include "../snddef.h"
#include "emutypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum EOPLL_TONE_ENUM {EOPLL_2413_TONE=0, EOPLL_VRC7_TONE=1, EOPLL_281B_TONE=2} ;

/* voice data */
typedef struct __EOPLL_PATCH {
  uint8_t TL,FB,EG,ML,AR,DR,SL,RR,KR,KL,AM,PM,WF ;
} EOPLL_PATCH ;

/* slot */
typedef struct __EOPLL_SLOT {

  const EOPLL_PATCH *patch;

  uint8_t type ;          /* 0 : modulator 1 : carrier */

  /* OUTPUT */
  int16_t feedback ;
  int16_t output[2] ;   /* Output value of slot */

  /* for Phase Generator (PG) */
  uint16_t *sintbl ;    /* Wavetable */
  uint32_t phase ;      /* Phase */
  uint32_t dphase ;     /* Phase increment amount */
  uint32_t pgout ;      /* output */

  /* for Envelope Generator (EG) */
  uint16_t fnum ;         /* F-Number */
  uint8_t block ;         /* Block */
  uint8_t volume ;        /* Current volume */
  uint8_t sustine ;       /* Sustine 1 = ON, 0 = OFF */
  uint32_t tll ;          /* Total Level + Key scale level*/
  uint32_t rks ;        /* Key scale offset (Rks) */
  int32_t eg_mode ;       /* Current state */
  uint32_t eg_phase ;   /* Phase */
  uint32_t eg_dphase ;  /* Phase increment amount */
  uint32_t egout ;      /* output */

} EOPLL_SLOT ;

/* Mask */
#define EOPLL_MASK_CH(x) (1<<(x))
#define EOPLL_MASK_HH (1<<(9))
#define EOPLL_MASK_CYM (1<<(10))
#define EOPLL_MASK_TOM (1<<(11))
#define EOPLL_MASK_SD (1<<(12))
#define EOPLL_MASK_BD (1<<(13))
#define EOPLL_MASK_RHYTHM ( EOPLL_MASK_HH | EOPLL_MASK_CYM | EOPLL_MASK_TOM | EOPLL_MASK_SD | EOPLL_MASK_BD )

/* opll */
typedef struct __EOPLL {

  DEV_DATA _devData;

  /* Input clock */
  uint32_t clk;
  /* Sampling rate */
  uint32_t rate;

  uint8_t vrc7_mode;
  uint8_t adr ;
  //int32_t out ;

  uint32_t realstep ;
  uint32_t oplltime ;
  uint32_t opllstep ;
  //int32_t prev, next ;
  int32_t sprev[2],snext[2];
  int32_t pan[15][2];

  /* Register */
  uint8_t reg[0x40] ;
  uint8_t slot_on_flag[18] ;

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
  uint8_t patch_number[9];
  uint8_t key_status[9] ;

  /* Slot */
  EOPLL_SLOT slot[18] ;

  /* Voice Data */
  EOPLL_PATCH patch[19*2] ;
  //uint8_t patch_update[2] ; /* flag for check patch update */

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

} EOPLL ;

/* Create Object */
static UINT8 device_start_ym2413_emu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
EOPLL *EOPLL_new(uint32_t clk, uint32_t rate) ;
void EOPLL_delete(EOPLL *) ;

/* Setup */
void EOPLL_reset(EOPLL *) ;
void EOPLL_reset_patch(EOPLL *, int32_t) ;
void EOPLL_set_rate(EOPLL *opll, uint32_t r) ;
void EOPLL_set_quality(EOPLL *opll, uint8_t q) ;
void EOPLL_set_pan(EOPLL *, uint32_t ch, int16_t pan);
static void ym2413_pan_emu(void* chipptr, INT16* PanVals);

/* Port/Register access */
void EOPLL_writeIO(EOPLL *, UINT8 reg, UINT8 val) ;
void EOPLL_writeReg(EOPLL *, UINT8 reg, UINT8 val) ;

/* Synthsize */
int16_t EOPLL_calc(EOPLL *) ;
void EOPLL_calc_stereo (EOPLL * opll, UINT32 samples, DEV_SMPL **out) ;

/* Misc */
void EOPLL_setPatch(EOPLL *, const uint8_t *dump) ;
void EOPLL_copyPatch(EOPLL *, int32_t, EOPLL_PATCH *) ;
void EOPLL_forceRefresh(EOPLL *) ;
/* Utility */
void EOPLL_dump2patch(const uint8_t *dump, EOPLL_PATCH *patch) ;
void EOPLL_patch2dump(const EOPLL_PATCH *patch, uint8_t *dump) ;
void EOPLL_getDefaultPatch(int32_t type, int32_t num, EOPLL_PATCH *) ;

/* Channel Mask */
uint32_t EOPLL_setMask(EOPLL *, uint32_t mask) ;
uint32_t EOPLL_toggleMask(EOPLL *, uint32_t mask) ;
void EOPLL_SetMuteMask(EOPLL* opll, UINT32 MuteMask);
void EOPLL_SetChipMode(EOPLL* opll, UINT8 Mode);

#ifdef __cplusplus
}
#endif

#endif	// __EMU2413_H_PRIVATE__
