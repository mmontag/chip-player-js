#ifndef __EMU2413_H_PRIVATE__
#define __EMU2413_H_PRIVATE__

#include "emutypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EOPLL_DEBUG 0

enum EOPLL_TONE_ENUM { EOPLL_2413_TONE = 0, EOPLL_VRC7_TONE = 1, EOPLL_281B_TONE = 2 };

/* voice data */
typedef struct __EOPLL_PATCH {
  uint32_t TL, FB, EG, ML, AR, DR, SL, RR, KR, KL, AM, PM, WS;
} EOPLL_PATCH;

/* slot */
typedef struct __EOPLL_SLOT {
  uint8_t number;

  /* type flags:
   * 000000SM 
   *       |+-- M: 0:modulator 1:carrier
   *       +--- S: 0:normal 1:single slot mode (sd, tom, hh or cym) 
   */
  uint8_t type;

  const EOPLL_PATCH *patch; /* voice parameter */

  /* slot output */
  int32_t output[2]; /* output value, latest and previous. */

  /* phase generator (pg) */
  const uint16_t *wave_table; /* wave table */
  uint32_t pg_phase;    /* pg phase */
  uint32_t pg_out;      /* pg output, as index of wave table */
  uint8_t pg_keep;      /* if 1, pg_phase is preserved when key-on */
  uint16_t blk_fnum;    /* (block << 9) | f-number */
  uint16_t fnum;        /* f-number (9 bits) */
  uint8_t blk;          /* block (3 bits) */

  /* envelope generator (eg) */
  uint8_t eg_state;  /* current state */
  int32_t volume;    /* current volume */
  uint8_t key_flag;  /* key-on flag 1:on 0:off */
  uint8_t sus_flag;  /* key-sus option 1:on 0:off */
  uint16_t tll;      /* total level + key scale level*/
  uint8_t rks;       /* key scale offset (rks) for eg speed */
  uint8_t eg_rate_h; /* eg speed rate high 4bits */
  uint8_t eg_rate_l; /* eg speed rate low 2bits */
  uint32_t eg_shift; /* shift for eg global counter, controls envelope speed */
  uint32_t eg_out;   /* eg output */

  uint32_t update_requests; /* flags to debounce update */

#if EOPLL_DEBUG
  uint8_t last_eg_state;
#endif
} EOPLL_SLOT;

/* mask */
#define EOPLL_MASK_CH(x) (1 << (x))
#define EOPLL_MASK_HH (1 << (9))
#define EOPLL_MASK_CYM (1 << (10))
#define EOPLL_MASK_TOM (1 << (11))
#define EOPLL_MASK_SD (1 << (12))
#define EOPLL_MASK_BD (1 << (13))
#define EOPLL_MASK_RHYTHM (EOPLL_MASK_HH | EOPLL_MASK_CYM | EOPLL_MASK_TOM | EOPLL_MASK_SD | EOPLL_MASK_BD)

/* rate conveter */
typedef struct __EOPLL_RateConv {
  int ch;
  double timer;
  double f_ratio;
  int16_t *sinc_table;
  int32_t **buf;
} EOPLL_RateConv;

EOPLL_RateConv *EOPLL_RateConv_new(double f_inp, double f_out, int ch);
void EOPLL_RateConv_reset(EOPLL_RateConv *conv);
void EOPLL_RateConv_putData(EOPLL_RateConv *conv, int ch, int32_t data);
int32_t EOPLL_RateConv_getData(EOPLL_RateConv *conv, int ch);
void EOPLL_RateConv_delete(EOPLL_RateConv *conv);

typedef struct __EOPLL {
  DEV_DATA _devData;

  uint32_t clk;
  uint32_t rate;

  uint8_t chip_type;

  uint8_t adr;

  double inp_step;
  double out_step;
  double out_time;

  uint8_t reg[0x40];
  uint8_t test_flag;
  uint8_t rhythm_mode;
  uint32_t slot_key_status;

  uint32_t eg_counter;

  uint32_t pm_phase;
  int32_t am_phase;

  uint8_t lfo_am;

  uint32_t noise;
  uint8_t short_noise;

  int32_t patch_number[9];
  EOPLL_SLOT slot[18];
  EOPLL_PATCH patch[19 * 2];

  uint8_t pan[16];
  int32_t pan_fine[16][2];  /* [VB mod] changed from float to 16.16 fixed-point int32_t */
  uint32_t mask;

  /* channel output */
  /* 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym */
  int16_t ch_out[14];

  int32_t mix_out[2];

  EOPLL_RateConv *conv;
} EOPLL;

EOPLL *EOPLL_new(uint32_t clk, uint32_t rate);
void EOPLL_delete(EOPLL *);

void EOPLL_reset(EOPLL *);
void EOPLL_resetPatch(EOPLL *, uint8_t);

/** 
 * Set output wave sampling rate. 
 * @param rate sampling rate. If clock / 72 (typically 49716 or 49715 at 3.58MHz) is set, the internal rate converter is
 * disabled.
 */
void EOPLL_setRate(EOPLL *opll, uint32_t rate);

/** 
 * Set internal calcuration quality. Currently no effects, just for compatibility.
 * >= v1.0.0 always synthesizes internal output at clock/72 Hz.
 */
void EOPLL_setQuality(EOPLL *opll, uint8_t q);

/** 
 * Set pan pot (extra function - not YM2413 chip feature)
 * @param ch 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14,15:reserved
 * @param pan 0:mute 1:right 2:left 3:center 
 * ```
 * pan: 76543210
 *            |+- bit 1: enable Left output
 *            +-- bit 0: enable Right output
 * ```
 */
void EOPLL_setPan(EOPLL *opll, uint32_t ch, uint8_t pan);

/**
 * Set fine-grained panning
 * @param ch 0..8:tone 9:bd 10:hh 11:sd 12:tom 13:cym 14,15:reserved
 * @param pan [Valley Bell mod] pan position: -256 = left, 0 = centre, +256 = right
 */
void EOPLL_setPanFine(EOPLL *opll, uint32_t ch, int16_t pan);

/**
 * Set chip type. If vrc7 is selected, r#14 is ignored.
 * This method not change the current ROM patch set.
 * To change ROM patch set, use OPLL_resetPatch.
 * @param type 0:YM2413 1:VRC7
 */
void EOPLL_setChipType(EOPLL *opll, uint8_t type); 

void EOPLL_writeIO(EOPLL *opll, uint8_t reg, uint8_t val);
void EOPLL_writeReg(EOPLL *opll, uint8_t reg, uint8_t val);

/**
 * Calculate one sample
 */
int32_t EOPLL_calc(EOPLL *opll);

/**
 * Calulate stereo sample
 */
void EOPLL_calcStereo(EOPLL *opll, int32_t out[2]);

void EOPLL_setPatch(EOPLL *, const uint8_t *dump);
void EOPLL_copyPatch(EOPLL *, int32_t, EOPLL_PATCH *);

/** 
 * Force to refresh. 
 * External program should call this function after updating patch parameters.
 */
void EOPLL_forceRefresh(EOPLL *);

void EOPLL_dumpToPatch(const uint8_t *dump, EOPLL_PATCH *patch);
void EOPLL_patchToDump(const EOPLL_PATCH *patch, uint8_t *dump);
void EOPLL_getDefaultPatch(int32_t type, int32_t num, EOPLL_PATCH *);

/** 
 *  Set channel mask 
 *  @param mask mask flag: EOPLL_MASK_* can be used.
 *  - bit 0..8: mask for ch 1 to 9 (EOPLL_MASK_CH(i))
 *  - bit 9: mask for Hi-Hat (EOPLL_MASK_HH)
 *  - bit 10: mask for Top-Cym (EOPLL_MASK_CYM)
 *  - bit 11: mask for Tom (EOPLL_MASK_TOM)
 *  - bit 12: mask for Snare Drum (EOPLL_MASK_SD)
 *  - bit 13: mask for Bass Drum (EOPLL_MASK_BD)
 */
uint32_t EOPLL_setMask(EOPLL *, uint32_t mask);

/**
 * Toggler channel mask flag
 */
uint32_t EOPLL_toggleMask(EOPLL *, uint32_t mask);

/* for compatibility */
#define EOPLL_set_rate EOPLL_setRate
#define EOPLL_set_quality EOPLL_setQuality
#define EOPLL_set_pan EOPLL_setPan
#define EOPLL_set_pan_fine EOPLL_setPanFine
#define EOPLL_calc_stereo EOPLL_calcStereo
#define EOPLL_reset_patch EOPLL_resetPatch
#define EOPLL_dump2patch EOPLL_dumpToPatch
#define EOPLL_patch2dump EOPLL_patchToDump
#define EOPLL_setChipMode EOPLL_setChipType

#ifdef __cplusplus
}
#endif

#endif	// __EMU2413_H_PRIVATE__
