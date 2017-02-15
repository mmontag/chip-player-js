#ifndef __EMU2413_H__
#define __EMU2413_H__

#include <stdtype.h>
#include "../snddef.h"
#include "emutypes.h"

#define EMU2413_API

enum OPLL_TONE_ENUM {OPLL_2413_TONE=0, OPLL_VRC7_TONE=1, OPLL_281B_TONE=2} ;

/* voice data */
typedef struct __OPLL_PATCH {
  uint32_t TL,FB,EG,ML,AR,DR,SL,RR,KR,KL,AM,PM,WF ;
} OPLL_PATCH ;

/* slot */
typedef struct __OPLL_SLOT {

  OPLL_PATCH *patch;  

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

  void* chipInf;

  uint8_t vrc7_mode;
  uint8_t adr ;
  int32_t out ;

  float pan[14][2];

  /* Register */
  uint8_t reg[0x40] ;
  int32_t slot_on_flag[18] ;

  /* Pitch Modulator */
  uint32_t pm_phase ;
  int32_t lfo_pm ;

  /* Amp Modulator */
  int32_t am_phase ;
  int32_t lfo_am ;

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

  uint32_t mask ;

} OPLL ;

// Create Object
EMU2413_API OPLL *OPLL_new(uint32_t clk, uint32_t rate) ;
EMU2413_API void OPLL_delete(OPLL *) ;

// Setup
EMU2413_API void OPLL_reset(OPLL *) ;
EMU2413_API void OPLL_reset_patch(OPLL *, int32_t) ;
EMU2413_API void OPLL_set_rate(OPLL *opll, uint32_t r) ;
EMU2413_API void OPLL_set_pan(OPLL *, uint32_t ch, int32_t pan);

// Port/Register access
EMU2413_API void OPLL_writeIO(OPLL *, UINT8 reg, UINT8 val) ;
EMU2413_API void OPLL_writeReg(OPLL *, UINT8 reg, UINT8 val) ;

// Synthsize
EMU2413_API void OPLL_calc_stereo (OPLL * opll, UINT32 samples, DEV_SMPL **out) ;

// Misc
EMU2413_API void OPLL_setPatch(OPLL *, const uint8_t *dump) ;
EMU2413_API void OPLL_copyPatch(OPLL *, int32_t, OPLL_PATCH *) ;
EMU2413_API void OPLL_forceRefresh(OPLL *) ;
// Utility
EMU2413_API void OPLL_dump2patch(const uint8_t *dump, OPLL_PATCH *patch) ;
EMU2413_API void OPLL_patch2dump(const OPLL_PATCH *patch, uint8_t *dump) ;
EMU2413_API void OPLL_getDefaultPatch(int32_t type, int32_t num, OPLL_PATCH *) ;

// Channel Mask
EMU2413_API void OPLL_SetMuteMask(OPLL* opll, UINT32 MuteMask);
EMU2413_API void OPLL_SetChipMode(OPLL* opll, UINT8 Mode);

#endif	// __EMU2413_H__
