/*****************************************************************************
 *
 *  POKEY chip emulator 4.51
 *  Copyright Nicola Salmoria and the MAME Team
 *
 *  Based on original info found in Ron Fries' Pokey emulator,
 *  with additions by Brad Oliver, Eric Smith and Juergen Buchmueller,
 *  paddle (a/d conversion) details from the Atari 400/800 Hardware Manual.
 *  Polynome algorithms according to info supplied by Perry McFarlane.
 *
 *  This code is subject to the MAME license, which besides other
 *  things means it is distributed as is, no warranties whatsoever.
 *  For more details read mame.txt that comes with MAME.
 *
 *  4.51:
 *  - changed to use the attotime datatype
 *  4.5:
 *  - changed the 9/17 bit polynomial formulas such that the values
 *    required for the Tempest Pokey protection will be found.
 *    Tempest expects the upper 4 bits of the RNG to appear in the
 *    lower 4 bits after four cycles, so there has to be a shift
 *    of 1 per cycle (which was not the case before). Bits #6-#13 of the
 *    new RNG give this expected result now, bits #0-7 of the 9 bit poly.
 *  - reading the RNG returns the shift register contents ^ 0xff.
 *    That way resetting the Pokey with SKCTL (which resets the
 *    polynome shifters to 0) returns the expected 0xff value.
 *  4.4:
 *  - reversed sample values to make OFF channels produce a zero signal.
 *    actually de-reversed them; don't remember that I reversed them ;-/
 *  4.3:
 *  - for POT inputs returning zero, immediately assert the ALLPOT
 *    bit after POTGO is written, otherwise start trigger timer
 *    depending on SK_PADDLE mode, either 1-228 scanlines or 1-2
 *    scanlines, depending on the SK_PADDLE bit of SKCTL.
 *  4.2:
 *  - half volume for channels which are inaudible (this should be
 *    close to the real thing).
 *  4.1:
 *  - default gain increased to closely match the old code.
 *  - random numbers repeat rate depends on POLY9 flag too!
 *  - verified sound output with many, many Atari 800 games,
 *    including the SUPPRESS_INAUDIBLE optimizations.
 *  4.0:
 *  - rewritten from scratch.
 *  - 16bit stream interface.
 *  - serout ready/complete delayed interrupts.
 *  - reworked pot analog/digital conversion timing.
 *  - optional non-indexing pokey update functions.
 *
 *****************************************************************************/

#include <stdlib.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "pokey.h"


static void pokey_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_pokey(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_pokey(void *info);
static void device_reset_pokey(void *info);

static UINT8 pokey_r(void *info, UINT8 offset);
static void pokey_w(void *info, UINT8 offset, UINT8 data);

static void pokey_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, pokey_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, pokey_r},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"POKEY", "MAME", FCC_MAME,
	
	device_start_pokey,
	device_stop_pokey,
	device_reset_pokey,
	pokey_update,
	
	NULL,	// SetOptionBits
	pokey_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_Pokey[] =
{
	&devDef,
	NULL
};


/* CONSTANT DEFINITIONS */

/* POKEY WRITE LOGICALS */
#define AUDF1_C		0x00
#define AUDC1_C		0x01
#define AUDF2_C		0x02
#define AUDC2_C		0x03
#define AUDF3_C		0x04
#define AUDC3_C		0x05
#define AUDF4_C		0x06
#define AUDC4_C		0x07
#define AUDCTL_C	0x08
#define STIMER_C	0x09
#define SKREST_C	0x0A
#define POTGO_C		0x0B
#define SEROUT_C	0x0D
#define IRQEN_C		0x0E
#define SKCTL_C		0x0F

/* POKEY READ LOGICALS */
#define POT0_C		0x00
#define POT1_C		0x01
#define POT2_C		0x02
#define POT3_C		0x03
#define POT4_C		0x04
#define POT5_C		0x05
#define POT6_C		0x06
#define POT7_C		0x07
#define ALLPOT_C	0x08
#define KBCODE_C	0x09
#define RANDOM_C	0x0A
#define SERIN_C		0x0D
#define IRQST_C		0x0E
#define SKSTAT_C	0x0F

/* exact 1.79 MHz clock freq (of the Atari 800 that is) */
#define FREQ_17_EXACT	1789790


/*
 * Defining this produces much more (about twice as much)
 * but also more efficient code. Ideally this should be set
 * for processors with big code cache and for healthy compilers :)
 */
#ifndef BIG_SWITCH
#ifndef HEAVY_MACRO_USAGE
#define HEAVY_MACRO_USAGE	1
#endif
#else
#define HEAVY_MACRO_USAGE	BIG_SWITCH
#endif

#define SUPPRESS_INAUDIBLE	1

/* Four channels with a range of 0..32767 and volume 0..15 */
//#define POKEY_DEFAULT_GAIN (32767/15/4)

/*
 * But we raise the gain and risk clipping, the old Pokey did
 * this too. It defined POKEY_DEFAULT_GAIN 6 and this was
 * 6 * 15 * 4 = 360, 360/256 = 1.40625
 * I use 15/11 = 1.3636, so this is a little lower.
 */
#define POKEY_DEFAULT_GAIN (32767/11/4)

#define VERBOSE 		0
#define VERBOSE_SOUND	0
#define VERBOSE_TIMER	0
#define VERBOSE_POLY	0
#define VERBOSE_RAND	0

#define LOG(x) do { if (VERBOSE) logerror x; } while (0)

#define LOG_SOUND(x) do { if (VERBOSE_SOUND) logerror x; } while (0)

#define LOG_TIMER(x) do { if (VERBOSE_TIMER) logerror x; } while (0)

#define LOG_POLY(x) do { if (VERBOSE_POLY) logerror x; } while (0)

#define LOG_RAND(x) do { if (VERBOSE_RAND) logerror x; } while (0)

#define CHAN1	0
#define CHAN2	1
#define CHAN3	2
#define CHAN4	3

#define TIMER1	0
#define TIMER2	1
#define TIMER4	2

/* values to add to the divisors for the different modes */
#define DIVADD_LOCLK		1
#define DIVADD_HICLK		4
#define DIVADD_HICLK_JOINED 7

/* AUDCx */
#define NOTPOLY5	0x80	/* selects POLY5 or direct CLOCK */
#define POLY4		0x40	/* selects POLY4 or POLY17 */
#define PURE		0x20	/* selects POLY4/17 or PURE tone */
#define VOLUME_ONLY 0x10	/* selects VOLUME OUTPUT ONLY */
#define VOLUME_MASK 0x0f	/* volume mask */

/* AUDCTL */
#define POLY9		0x80	/* selects POLY9 or POLY17 */
#define CH1_HICLK	0x40	/* selects 1.78979 MHz for Ch 1 */
#define CH3_HICLK	0x20	/* selects 1.78979 MHz for Ch 3 */
#define CH12_JOINED 0x10	/* clocks channel 1 w/channel 2 */
#define CH34_JOINED 0x08	/* clocks channel 3 w/channel 4 */
#define CH1_FILTER	0x04	/* selects channel 1 high pass filter */
#define CH2_FILTER	0x02	/* selects channel 2 high pass filter */
#define CLK_15KHZ	0x01	/* selects 15.6999 kHz or 63.9211 kHz */

/* IRQEN (D20E) */
#define IRQ_BREAK	0x80	/* BREAK key pressed interrupt */
#define IRQ_KEYBD	0x40	/* keyboard data ready interrupt */
#define IRQ_SERIN	0x20	/* serial input data ready interrupt */
#define IRQ_SEROR	0x10	/* serial output register ready interrupt */
#define IRQ_SEROC	0x08	/* serial output complete interrupt */
#define IRQ_TIMR4	0x04	/* timer channel #4 interrupt */
#define IRQ_TIMR2	0x02	/* timer channel #2 interrupt */
#define IRQ_TIMR1	0x01	/* timer channel #1 interrupt */

/* SKSTAT (R/D20F) */
#define SK_FRAME	0x80	/* serial framing error */
#define SK_OVERRUN	0x40	/* serial overrun error */
#define SK_KBERR	0x20	/* keyboard overrun error */
#define SK_SERIN	0x10	/* serial input high */
#define SK_SHIFT	0x08	/* shift key pressed */
#define SK_KEYBD	0x04	/* keyboard key pressed */
#define SK_SEROUT	0x02	/* serial output active */

/* SKCTL (W/D20F) */
#define SK_BREAK	0x80	/* serial out break signal */
#define SK_BPS		0x70	/* bits per second */
#define SK_FM		0x08	/* FM mode */
#define SK_PADDLE	0x04	/* fast paddle a/d conversion */
#define SK_RESET	0x03	/* reset serial/keyboard interface */

#define DIV_64		28		 /* divisor for 1.78979 MHz clock to 63.9211 kHz */
#define DIV_15		114 	 /* divisor for 1.78979 MHz clock to 15.6999 kHz */

typedef struct _pokey_state pokey_state;
struct _pokey_state
{
	DEV_DATA _devData;

	INT32 counter[4];		/* channel counter */
	INT32 divisor[4];		/* channel divisor (modulo value) */
	UINT32 volume[4];		/* channel volume - derived */
	UINT8 output[4];		/* channel output signal (1 active, 0 inactive) */
	UINT8 audible[4];		/* channel plays an audible tone/effect */
	UINT8 Muted[4];
	UINT32 samplerate_24_8;	/* sample rate in 24.8 format */
	UINT32 samplepos_fract;	/* sample position fractional part */
	UINT32 samplepos_whole;	/* sample position whole part */
	UINT32 polyadjust;		/* polynome adjustment */
	UINT32 p4;				/* poly4 index */
	UINT32 p5;				/* poly5 index */
	UINT32 p9;				/* poly9 index */
	UINT32 p17;				/* poly17 index */
	UINT32 r9;				/* rand9 index */
	UINT32 r17;				/* rand17 index */
	UINT32 clockmult;		/* clock multiplier */
	//emu_timer *timer[3];	/* timers for channel 1,2 and 4 events */
	double timer_period[3];	/* computed periods for these timers */
	UINT8 timer_param[3];	/* computed parameters for these timers */
	//emu_timer *rtimer;	/* timer for calculating the random offset */
	//void (*interrupt_cb)(void* chip, int mask);
	UINT8 AUDF[4];			/* AUDFx (D200, D202, D204, D206) */
	UINT8 AUDC[4];			/* AUDCx (D201, D203, D205, D207) */
	UINT8 POTx[8];			/* POTx   (R/D200-D207) */
	UINT8 AUDCTL;			/* AUDCTL (W/D208) */
	UINT8 ALLPOT;			/* ALLPOT (R/D208) */
	UINT8 KBCODE;			/* KBCODE (R/D209) */
	UINT8 RANDOM;			/* RANDOM (R/D20A) */
	UINT8 SERIN;			/* SERIN  (R/D20D) */
	UINT8 SEROUT;			/* SEROUT (W/D20D) */
	UINT8 IRQST;			/* IRQST  (R/D20E) */
	UINT8 IRQEN;			/* IRQEN  (W/D20E) */
	UINT8 SKSTAT;			/* SKSTAT (R/D20F) */
	UINT8 SKCTL;			/* SKCTL  (W/D20F) */
	double clock_period;

	UINT8 poly4[0x0f];
	UINT8 poly5[0x1f];
	UINT8 poly9[0x1ff];
	UINT8 poly17[0x1ffff];

	UINT8 rand9[0x1ff];
	UINT8 rand17[0x1ffff];
};


#define P4(chip)  chip->poly4[chip->p4]
#define P5(chip)  chip->poly5[chip->p5]
#define P9(chip)  chip->poly9[chip->p9]
#define P17(chip) chip->poly17[chip->p17]


#define SAMPLE	-1

#define ADJUST_EVENT(chip)												\
	chip->counter[CHAN1] -= event;										\
	chip->counter[CHAN2] -= event;										\
	chip->counter[CHAN3] -= event;										\
	chip->counter[CHAN4] -= event;										\
	chip->samplepos_whole -= event;										\
	chip->polyadjust += event

#if SUPPRESS_INAUDIBLE

#define PROCESS_CHANNEL(chip,ch)										\
	int toggle = 0;														\
	ADJUST_EVENT(chip);													\
	/* reset the channel counter */ 									\
	if( chip->audible[ch] )												\
		chip->counter[ch] = chip->divisor[ch];							\
	else																\
		chip->counter[ch] = 0x7fffffff;									\
	chip->p4 = (chip->p4+chip->polyadjust)%0x0000f;						\
	chip->p5 = (chip->p5+chip->polyadjust)%0x0001f;						\
	chip->p9 = (chip->p9+chip->polyadjust)%0x001ff;						\
	chip->p17 = (chip->p17+chip->polyadjust)%0x1ffff;					\
	chip->polyadjust = 0;												\
	if( (chip->AUDC[ch] & NOTPOLY5) || P5(chip) )						\
	{																	\
		if( chip->AUDC[ch] & PURE )										\
			toggle = 1;													\
		else															\
		if( chip->AUDC[ch] & POLY4 )									\
			toggle = chip->output[ch] == !P4(chip);						\
		else															\
		if( chip->AUDCTL & POLY9 )										\
			toggle = chip->output[ch] == !P9(chip);						\
		else															\
			toggle = chip->output[ch] == !P17(chip);					\
	}																	\
	if( toggle )														\
	{																	\
		if( chip->audible[ch] && ! chip->Muted[ch] )					\
		{																\
			if( chip->output[ch] )										\
				sum -= chip->volume[ch];								\
			else														\
				sum += chip->volume[ch];								\
		}																\
		chip->output[ch] ^= 1;											\
	}																	\
	/* is this a filtering channel (3/4) and is the filter active? */	\
	if( chip->AUDCTL & ((CH1_FILTER|CH2_FILTER) & (0x10 >> ch)) )		\
	{																	\
		if( chip->output[ch-2] )										\
		{																\
			chip->output[ch-2] = 0;										\
			if( chip->audible[ch] && ! chip->Muted[ch] )				\
				sum -= chip->volume[ch-2];								\
		}																\
	}																	\

#else

#define PROCESS_CHANNEL(chip,ch)										\
	int toggle = 0;														\
	ADJUST_EVENT(chip);													\
	/* reset the channel counter */ 									\
	chip->counter[ch] = p[chip].divisor[ch];							\
	chip->p4 = (chip->p4+chip->polyadjust)%0x0000f;						\
	chip->p5 = (chip->p5+chip->polyadjust)%0x0001f;						\
	chip->p9 = (chip->p9+chip->polyadjust)%0x001ff;						\
	chip->p17 = (chip->p17+chip->polyadjust)%0x1ffff;					\
	chip->polyadjust = 0;												\
	if( (chip->AUDC[ch] & NOTPOLY5) || P5(chip) )						\
	{																	\
		if( chip->AUDC[ch] & PURE )										\
			toggle = 1;													\
		else															\
		if( chip->AUDC[ch] & POLY4 )									\
			toggle = chip->output[ch] == !P4(chip);						\
		else															\
		if( chip->AUDCTL & POLY9 )										\
			toggle = chip->output[ch] == !P9(chip);						\
		else															\
			toggle = chip->output[ch] == !P17(chip);					\
	}																	\
	if( toggle && ! chip->Muted[ch] )									\
	{																	\
		if( chip->output[ch] )											\
			sum -= chip->volume[ch];									\
		else															\
			sum += chip->volume[ch];									\
		chip->output[ch] ^= 1;											\
	}																	\
	/* is this a filtering channel (3/4) and is the filter active? */	\
	if( chip->AUDCTL & ((CH1_FILTER|CH2_FILTER) & (0x10 >> ch)) )		\
	{																	\
		if( chip->output[ch-2] && ! chip->Muted[ch] )					\
		{																\
			chip->output[ch-2] = 0;										\
			sum -= chip->volume[ch-2];									\
		}																\
	}																	\

#endif

#define PROCESS_SAMPLE(chip)											\
	ADJUST_EVENT(chip);													\
	/* adjust the sample position */									\
	chip->samplepos_whole++;											\
	/* store sum of output signals into the buffer */					\
	bufL[i] = bufR[i] = sum;

#if HEAVY_MACRO_USAGE

/*
 * This version of PROCESS_POKEY repeats the search for the minimum
 * event value without using an index to the channel. That way the
 * PROCESS_CHANNEL macros can be called with fixed values and expand
 * to much more efficient code
 */

#define PROCESS_POKEY(chip) 											\
	UINT32 i = 0;														\
	UINT32 sum = 0;														\
	if( chip->output[CHAN1] && ! chip->Muted[CHAN1] )					\
		sum += chip->volume[CHAN1];										\
	if( chip->output[CHAN2] && ! chip->Muted[CHAN2] )					\
		sum += chip->volume[CHAN2];										\
	if( chip->output[CHAN3] && ! chip->Muted[CHAN3] )					\
		sum += chip->volume[CHAN3];										\
	if( chip->output[CHAN4] && ! chip->Muted[CHAN4] )					\
		sum += chip->volume[CHAN4];										\
	for( i = 0; i < samples; i++ )										\
	{																	\
		if( chip->counter[CHAN1] < chip->samplepos_whole )				\
		{																\
			if( chip->counter[CHAN2] <  chip->counter[CHAN1] )			\
			{															\
				if( chip->counter[CHAN3] <  chip->counter[CHAN2] )		\
				{														\
					if( chip->counter[CHAN4] < chip->counter[CHAN3] )	\
					{													\
						UINT32 event = chip->counter[CHAN4];			\
						PROCESS_CHANNEL(chip,CHAN4);					\
					}													\
					else												\
					{													\
						UINT32 event = chip->counter[CHAN3];			\
						PROCESS_CHANNEL(chip,CHAN3);					\
					}													\
				}														\
				else													\
				if( chip->counter[CHAN4] < chip->counter[CHAN2] )		\
				{														\
					UINT32 event = chip->counter[CHAN4];				\
					PROCESS_CHANNEL(chip,CHAN4);						\
				}														\
				else													\
				{														\
					UINT32 event = chip->counter[CHAN2];				\
					PROCESS_CHANNEL(chip,CHAN2);						\
				}														\
			}															\
			else														\
			if( chip->counter[CHAN3] < chip->counter[CHAN1] )			\
			{															\
				if( chip->counter[CHAN4] < chip->counter[CHAN3] )		\
				{														\
					UINT32 event = chip->counter[CHAN4];				\
					PROCESS_CHANNEL(chip,CHAN4);						\
				}														\
				else													\
				{														\
					UINT32 event = chip->counter[CHAN3];				\
					PROCESS_CHANNEL(chip,CHAN3);						\
				}														\
			}															\
			else														\
			if( chip->counter[CHAN4] < chip->counter[CHAN1] )			\
			{															\
				UINT32 event = chip->counter[CHAN4];					\
				PROCESS_CHANNEL(chip,CHAN4);							\
			}															\
			else														\
			{															\
				UINT32 event = chip->counter[CHAN1];					\
				PROCESS_CHANNEL(chip,CHAN1);							\
			}															\
		}																\
		else															\
		if( chip->counter[CHAN2] < chip->samplepos_whole )				\
		{																\
			if( chip->counter[CHAN3] < chip->counter[CHAN2] )			\
			{															\
				if( chip->counter[CHAN4] < chip->counter[CHAN3] )		\
				{														\
					UINT32 event = chip->counter[CHAN4];				\
					PROCESS_CHANNEL(chip,CHAN4);						\
				}														\
				else													\
				{														\
					UINT32 event = chip->counter[CHAN3];				\
					PROCESS_CHANNEL(chip,CHAN3);						\
				}														\
			}															\
			else														\
			if( chip->counter[CHAN4] < chip->counter[CHAN2] )			\
			{															\
				UINT32 event = chip->counter[CHAN4];					\
				PROCESS_CHANNEL(chip,CHAN4);							\
			}															\
			else														\
			{															\
				UINT32 event = chip->counter[CHAN2];					\
				PROCESS_CHANNEL(chip,CHAN2);							\
			}															\
		}																\
		else															\
		if( chip->counter[CHAN3] < chip->samplepos_whole )				\
		{																\
			if( chip->counter[CHAN4] < chip->counter[CHAN3] )			\
			{															\
				UINT32 event = chip->counter[CHAN4];					\
				PROCESS_CHANNEL(chip,CHAN4);							\
			}															\
			else														\
			{															\
				UINT32 event = chip->counter[CHAN3];					\
				PROCESS_CHANNEL(chip,CHAN3);							\
			}															\
		}																\
		else															\
		if( chip->counter[CHAN4] < chip->samplepos_whole )				\
		{																\
			UINT32 event = chip->counter[CHAN4];						\
			PROCESS_CHANNEL(chip,CHAN4);								\
		}																\
		else															\
		{																\
			UINT32 event = chip->samplepos_whole;						\
			PROCESS_SAMPLE(chip);										\
		}																\
	}																	\
	/*chip->rtimer->adjust(attotime::never)*/

#else	/* no HEAVY_MACRO_USAGE */
/*
 * And this version of PROCESS_POKEY uses event and channel variables
 * so that the PROCESS_CHANNEL macro needs to index memory at runtime.
 */

#define PROCESS_POKEY(chip)												\
	UINT32 i = 0;														\
	UINT32 sum = 0;														\
	if( chip->output[CHAN1] && ! chip->Muted[CHAN1] )					\
		sum += chip->volume[CHAN1];										\
	if( chip->output[CHAN2] && ! chip->Muted[CHAN2] )					\
		sum += chip->volume[CHAN2];										\
	if( chip->output[CHAN3] && ! chip->Muted[CHAN3] )					\
		sum += chip->volume[CHAN3];										\
	if( chip->output[CHAN4] && ! chip->Muted[CHAN4] )					\
		sum += chip->volume[CHAN4];										\
	while( samples > 0 )												\
	{																	\
		UINT32 event = chip->samplepos_whole;							\
		UINT32 channel = SAMPLE;										\
		if( chip->counter[CHAN1] < event )								\
		{																\
			event = chip->counter[CHAN1];								\
			channel = CHAN1;											\
		}																\
		if( chip->counter[CHAN2] < event )								\
		{																\
			event = chip->counter[CHAN2];								\
			channel = CHAN2;											\
		}																\
		if( chip->counter[CHAN3] < event )								\
		{																\
			event = chip->counter[CHAN3];								\
			channel = CHAN3;											\
		}																\
		if( chip->counter[CHAN4] < event )								\
		{																\
			event = chip->counter[CHAN4];								\
			channel = CHAN4;											\
		}																\
		if( channel == SAMPLE )											\
		{																\
			PROCESS_SAMPLE(chip);										\
		}																\
		else															\
		{																\
			PROCESS_CHANNEL(chip,channel);								\
		}																\
	}																	\
	/*chip->rtimer->adjust(attotime::never)*/

#endif


static void pokey_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	pokey_state *chip = (pokey_state *)param;
	DEV_SMPL *bufL = outputs[0];
	DEV_SMPL *bufR = outputs[1];
	PROCESS_POKEY(chip);
}


static void poly_init(UINT8 *poly, int size, int left, int right, int add)
{
	int mask = (1 << size) - 1;
	int i, x = 0;

	LOG_POLY(("poly %d\n", size));
	for( i = 0; i < mask; i++ )
	{
		*poly++ = x & 1;
		LOG_POLY(("%05x: %d\n", x, x&1));
		/* calculate next bit */
		x = ((x << left) + (x >> right) + add) & mask;
	}
}

static void rand_init(UINT8 *rng, int size, int left, int right, int add)
{
	int mask = (1 << size) - 1;
	int i, x = 0;

	LOG_RAND(("rand %d\n", size));
	for( i = 0; i < mask; i++ )
	{
		if (size == 17)
			*rng = x >> 6;	/* use bits 6..13 */
		else
			*rng = x;		/* use bits 0..7 */
		LOG_RAND(("%05x: %02x\n", x, *rng));
		rng++;
		/* calculate next bit */
		x = ((x << left) + (x >> right) + add) & mask;
	}
}


static UINT8 device_start_pokey(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	pokey_state *chip;
	UINT32 sample_rate;

	sample_rate = cfg->clock;

	chip = (pokey_state *)calloc(1, sizeof(pokey_state));
	if (chip == NULL)
		return 0xFF;

	chip->clock_period = 1.0 / cfg->clock;

	/* initialize the poly counters */
	poly_init(chip->poly4,   4, 3, 1, 0x00004);
	poly_init(chip->poly5,   5, 3, 2, 0x00008);
	poly_init(chip->poly9,   9, 8, 1, 0x00180);
	poly_init(chip->poly17, 17,16, 1, 0x1c000);

	/* initialize the random arrays */
	rand_init(chip->rand9,   9, 8, 1, 0x00180);
	rand_init(chip->rand17, 17,16, 1, 0x1c000);

	chip->samplerate_24_8 = (cfg->clock << 8) / sample_rate;
	chip->divisor[CHAN1] = 4;
	chip->divisor[CHAN2] = 4;
	chip->divisor[CHAN3] = 4;
	chip->divisor[CHAN4] = 4;
	chip->clockmult = DIV_64;
	chip->KBCODE = 0x09;	/* Atari 800 'no key' */
	chip->SKCTL = SK_RESET;	/* let the RNG run after reset */

	//chip->timer[0] = device->machine().scheduler().timer_alloc(FUNC(pokey_timer_expire), chip);
	//chip->timer[1] = device->machine().scheduler().timer_alloc(FUNC(pokey_timer_expire), chip);
	//chip->timer[2] = device->machine().scheduler().timer_alloc(FUNC(pokey_timer_expire), chip);
	//chip->interrupt_cb = NULL;

	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, sample_rate, &devDef);
	return 0x00;
}

static void device_stop_pokey(void *info)
{
	pokey_state *chip = (pokey_state *)info;
	
	free(chip);
	
	return;
}

static void device_reset_pokey(void *info)
{
	pokey_state *chip = (pokey_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
	{
		chip->counter[CurChn] = 0;
		chip->divisor[CurChn] = 4;
		chip->volume[CurChn] = 0;
		chip->output[CurChn] = 0;
		chip->audible[CurChn] = 0;
	}
	chip->samplepos_fract = 0;
	chip->samplepos_whole = 0;
	chip->polyadjust = 0;
	chip->p4 = 0;
	chip->p5 = 0;
	chip->p9 = 0;
	chip->p17 = 0;
	chip->r9 = 0;
	chip->r17 = 0;
	chip->clockmult = DIV_64;
	
	return;
}

static UINT8 pokey_r(void *info, UINT8 offset)
{
	pokey_state *p = (pokey_state *)info;
	int data = 0, pot;
	UINT32 adjust = 0;

	switch (offset & 15)
	{
	case POT0_C: case POT1_C: case POT2_C: case POT3_C:
	case POT4_C: case POT5_C: case POT6_C: case POT7_C:
		pot = offset & 7;
		break;

	case ALLPOT_C:
		/****************************************************************
		 * If the 2 least significant bits of SKCTL are 0, the ALLPOTs
		 * are disabled (SKRESET). Thanks to MikeJ for pointing this out.
		 ****************************************************************/
		if( (p->SKCTL & SK_RESET) == 0)
		{
			data = 0;
			//LOG(("POKEY '%s' ALLPOT internal $%02x (reset)\n", p->device->tag(), data));
		}
		else
		{
			data = p->ALLPOT;
			//LOG(("POKEY '%s' ALLPOT internal $%02x\n", p->device->tag(), data));
		}
		break;

	case KBCODE_C:
		data = p->KBCODE;
		break;

	case RANDOM_C:
		/****************************************************************
		 * If the 2 least significant bits of SKCTL are 0, the random
		 * number generator is disabled (SKRESET). Thanks to Eric Smith
		 * for pointing out this critical bit of info! If the random
		 * number generator is enabled, get a new random number. Take
		 * the time gone since the last read into account and read the
		 * new value from an appropriate offset in the rand17 table.
		 ****************************************************************/
		if( p->SKCTL & SK_RESET )
		{
			//adjust = p->rtimer->elapsed().as_double() / p->clock_period.as_double();
			adjust = 0;
			p->r9 = (p->r9 + adjust) % 0x001ff;
			p->r17 = (p->r17 + adjust) % 0x1ffff;
		}
		else
		{
			adjust = 1;
			p->r9 = 0;
			p->r17 = 0;
			//LOG_RAND(("POKEY '%s' rand17 frozen (SKCTL): $%02x\n", p->device->tag(), p->RANDOM));
		}
		if( p->AUDCTL & POLY9 )
		{
			p->RANDOM = p->rand9[p->r9];
			//LOG_RAND(("POKEY '%s' adjust %u rand9[$%05x]: $%02x\n", p->device->tag(), adjust, p->r9, p->RANDOM));
		}
		else
		{
			p->RANDOM = p->rand17[p->r17];
			//LOG_RAND(("POKEY '%s' adjust %u rand17[$%05x]: $%02x\n", p->device->tag(), adjust, p->r17, p->RANDOM));
		}
		//if (adjust > 0)
		//	p->rtimer->adjust(attotime::never);
		data = p->RANDOM ^ 0xff;
		break;

	case SERIN_C:
		data = p->SERIN;
		//LOG(("POKEY '%s' SERIN  $%02x\n", p->device->tag(), data));
		break;

	case IRQST_C:
		/* IRQST is an active low input port; we keep it active high */
		/* internally to ease the (un-)masking of bits */
		data = p->IRQST ^ 0xff;
		//LOG(("POKEY '%s' IRQST  $%02x\n", p->device->tag(), data));
		break;

	case SKSTAT_C:
		/* SKSTAT is also an active low input port */
		data = p->SKSTAT ^ 0xff;
		//LOG(("POKEY '%s' SKSTAT $%02x\n", p->device->tag(), data));
		break;

	default:
		//LOG(("POKEY '%s' register $%02x\n", p->device->tag(), offset));
		break;
	}
	return data;
}


static void pokey_w(void *info, UINT8 offset, UINT8 data)
{
	pokey_state *p = (pokey_state *)info;
	int ch_mask = 0, new_val;

	/* determine which address was changed */
	switch (offset & 15)
	{
	case AUDF1_C:
		if( data == p->AUDF[CHAN1] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDF1  $%02x\n", p->device->tag(), data));
		p->AUDF[CHAN1] = data;
		ch_mask = 1 << CHAN1;
		if( p->AUDCTL & CH12_JOINED )		/* if ch 1&2 tied together */
			ch_mask |= 1 << CHAN2;    /* then also change on ch2 */
		break;

	case AUDC1_C:
		if( data == p->AUDC[CHAN1] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDC1  $%02x (%s)\n", p->device->tag(), data, audc2str(data)));
		p->AUDC[CHAN1] = data;
		ch_mask = 1 << CHAN1;
		break;

	case AUDF2_C:
		if( data == p->AUDF[CHAN2] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDF2  $%02x\n", p->device->tag(), data));
		p->AUDF[CHAN2] = data;
		ch_mask = 1 << CHAN2;
		break;

	case AUDC2_C:
		if( data == p->AUDC[CHAN2] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDC2  $%02x (%s)\n", p->device->tag(), data, audc2str(data)));
		p->AUDC[CHAN2] = data;
		ch_mask = 1 << CHAN2;
		break;

	case AUDF3_C:
		if( data == p->AUDF[CHAN3] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDF3  $%02x\n", p->device->tag(), data));
		p->AUDF[CHAN3] = data;
		ch_mask = 1 << CHAN3;

		if( p->AUDCTL & CH34_JOINED )	/* if ch 3&4 tied together */
			ch_mask |= 1 << CHAN4;  /* then also change on ch4 */
		break;

	case AUDC3_C:
		if( data == p->AUDC[CHAN3] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDC3  $%02x (%s)\n", p->device->tag(), data, audc2str(data)));
		p->AUDC[CHAN3] = data;
		ch_mask = 1 << CHAN3;
		break;

	case AUDF4_C:
		if( data == p->AUDF[CHAN4] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDF4  $%02x\n", p->device->tag(), data));
		p->AUDF[CHAN4] = data;
		ch_mask = 1 << CHAN4;
		break;

	case AUDC4_C:
		if( data == p->AUDC[CHAN4] )
			return;
		//LOG_SOUND(("POKEY '%s' AUDC4  $%02x (%s)\n", p->device->tag(), data, audc2str(data)));
		p->AUDC[CHAN4] = data;
		ch_mask = 1 << CHAN4;
		break;

	case AUDCTL_C:
		if( data == p->AUDCTL )
			return;
		//LOG_SOUND(("POKEY '%s' AUDCTL $%02x (%s)\n", p->device->tag(), data, audctl2str(data)));
		p->AUDCTL = data;
		ch_mask = 15;		/* all channels */
		/* determine the base multiplier for the 'div by n' calculations */
		p->clockmult = (p->AUDCTL & CLK_15KHZ) ? DIV_15 : DIV_64;
		break;

	case STIMER_C:
		/* first remove any existing timers */
		//LOG_TIMER(("POKEY '%s' STIMER $%02x\n", p->device->tag(), data));

		//p->timer[TIMER1]->adjust(attotime::never, p->timer_param[TIMER1]);
		//p->timer[TIMER2]->adjust(attotime::never, p->timer_param[TIMER2]);
		//p->timer[TIMER4]->adjust(attotime::never, p->timer_param[TIMER4]);

		/* reset all counters to zero (side effect) */
		p->polyadjust = 0;
		p->counter[CHAN1] = 0;
		p->counter[CHAN2] = 0;
		p->counter[CHAN3] = 0;
		p->counter[CHAN4] = 0;

		/* joined chan#1 and chan#2 ? */
		if( p->AUDCTL & CH12_JOINED )
		{
			if( p->divisor[CHAN2] > 4 )
			{
				//LOG_TIMER(("POKEY '%s' timer1+2 after %d clocks\n", p->device->tag(), p->divisor[CHAN2]));
				// set timer #1 _and_ #2 event after timer_div clocks of joined CHAN1+CHAN2 //
				p->timer_period[TIMER2] = p->clock_period * p->divisor[CHAN2];
				p->timer_param[TIMER2] = IRQ_TIMR2|IRQ_TIMR1;
			}
		}
		else
		{
			if( p->divisor[CHAN1] > 4 )
			{
				//LOG_TIMER(("POKEY '%s' timer1 after %d clocks\n", p->device->tag(), p->divisor[CHAN1]));
				// set timer #1 event after timer_div clocks of CHAN1 //
				p->timer_period[TIMER1] = p->clock_period * p->divisor[CHAN1];
				p->timer_param[TIMER1] = IRQ_TIMR1;
			}

			if( p->divisor[CHAN2] > 4 )
			{
				//LOG_TIMER(("POKEY '%s' timer2 after %d clocks\n", p->device->tag(), p->divisor[CHAN2]));
				// set timer #2 event after timer_div clocks of CHAN2 //
				p->timer_period[TIMER2] = p->clock_period * p->divisor[CHAN2];
				p->timer_param[TIMER2] = IRQ_TIMR2;
			}
		}

		// Note: p[chip] does not have a timer #3 //

		if( p->AUDCTL & CH34_JOINED )
		{
			// not sure about this: if audc4 == 0000xxxx don't start timer 4 ? //
			if( p->AUDC[CHAN4] & 0xf0 )
			{
				if( p->divisor[CHAN4] > 4 )
				{
					//LOG_TIMER(("POKEY '%s' timer4 after %d clocks\n", p->device->tag(), p->divisor[CHAN4]));
					// set timer #4 event after timer_div clocks of CHAN4 //
					p->timer_period[TIMER4] = p->clock_period * p->divisor[CHAN4];
					p->timer_param[TIMER4] = IRQ_TIMR4;
				}
			}
		}
		else
		{
			if( p->divisor[CHAN4] > 4 )
			{
				//LOG_TIMER(("POKEY '%s' timer4 after %d clocks\n", p->device->tag(), p->divisor[CHAN4]));
				// set timer #4 event after timer_div clocks of CHAN4 //
				p->timer_period[TIMER4] = p->clock_period * p->divisor[CHAN4];
				p->timer_param[TIMER4] = IRQ_TIMR4;
			}
		}

		//p->timer[TIMER1]->enable(p->IRQEN & IRQ_TIMR1);
		//p->timer[TIMER2]->enable(p->IRQEN & IRQ_TIMR2);
		//p->timer[TIMER4]->enable(p->IRQEN & IRQ_TIMR4);*/
		break;

	case SKREST_C:
		/* reset SKSTAT */
		//LOG(("POKEY '%s' SKREST $%02x\n", p->device->tag(), data));
		p->SKSTAT &= ~(SK_FRAME|SK_OVERRUN|SK_KBERR);
		break;

	case POTGO_C:
		//LOG(("POKEY '%s' POTGO  $%02x\n", p->device->tag(), data));
		//pokey_potgo(p);
		break;

	case SEROUT_C:
		//LOG(("POKEY '%s' SEROUT $%02x\n", p->device->tag(), data));
		break;

	case IRQEN_C:
		//LOG(("POKEY '%s' IRQEN  $%02x\n", p->device->tag(), data));

		/* acknowledge one or more IRQST bits ? */
		if( p->IRQST & ~data )
		{
			/* reset IRQST bits that are masked now */
			p->IRQST &= data;
		}
		else
		{
			/* enable/disable timers now to avoid unneeded
			   breaking of the CPU cores for masked timers */
			//if( p->timer[TIMER1] && ((p->IRQEN^data) & IRQ_TIMR1) )
			//	p->timer[TIMER1]->enable(data & IRQ_TIMR1);
			//if( p->timer[TIMER2] && ((p->IRQEN^data) & IRQ_TIMR2) )
			//	p->timer[TIMER2]->enable(data & IRQ_TIMR2);
			//if( p->timer[TIMER4] && ((p->IRQEN^data) & IRQ_TIMR4) )
			//	p->timer[TIMER4]->enable(data & IRQ_TIMR4);
		}
		/* store irq enable */
		p->IRQEN = data;
		break;

	case SKCTL_C:
		if( data == p->SKCTL )
			return;
		//LOG(("POKEY '%s' SKCTL  $%02x\n", p->device->tag(), data));
		p->SKCTL = data;
		if( !(data & SK_RESET) )
		{
			pokey_w(p, IRQEN_C,  0);
			pokey_w(p, SKREST_C, 0);
		}
		break;
	}

	/************************************************************
	 * As defined in the manual, the exact counter values are
	 * different depending on the frequency and resolution:
	 *    64 kHz or 15 kHz - AUDF + 1
	 *    1.79 MHz, 8-bit  - AUDF + 4
	 *    1.79 MHz, 16-bit - AUDF[CHAN1]+256*AUDF[CHAN2] + 7
	 ************************************************************/

	/* only reset the channels that have changed */

	if( ch_mask & (1 << CHAN1) )
	{
		/* process channel 1 frequency */
		if( p->AUDCTL & CH1_HICLK )
			new_val = p->AUDF[CHAN1] + DIVADD_HICLK;
		else
			new_val = (p->AUDF[CHAN1] + DIVADD_LOCLK) * p->clockmult;

		//LOG_SOUND(("POKEY '%s' chan1 %d\n", p->device->tag(), new_val));

		p->volume[CHAN1] = (p->AUDC[CHAN1] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN1] = new_val;
		if( new_val < p->counter[CHAN1] )
			p->counter[CHAN1] = new_val;
		//if( p->interrupt_cb && p->timer[TIMER1] )
		//	p->timer[TIMER1]->adjust(p->clock_period * new_val, p->timer_param[TIMER1], p->timer_period[TIMER1]);
		p->audible[CHAN1] = !(
			(p->AUDC[CHAN1] & VOLUME_ONLY) ||
			(p->AUDC[CHAN1] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN1] & PURE) && new_val < (p->samplerate_24_8 >> 8)));
		if( !p->audible[CHAN1] )
		{
			p->output[CHAN1] = 1;
			p->counter[CHAN1] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN1] >>= 1;
		}
	}

	if( ch_mask & (1 << CHAN2) )
	{
		/* process channel 2 frequency */
		if( p->AUDCTL & CH12_JOINED )
		{
			if( p->AUDCTL & CH1_HICLK )
				new_val = p->AUDF[CHAN2] * 256 + p->AUDF[CHAN1] + DIVADD_HICLK_JOINED;
			else
				new_val = (p->AUDF[CHAN2] * 256 + p->AUDF[CHAN1] + DIVADD_LOCLK) * p->clockmult;
			//LOG_SOUND(("POKEY '%s' chan1+2 %d\n", p->device->tag(), new_val));
		}
		else
		{
			new_val = (p->AUDF[CHAN2] + DIVADD_LOCLK) * p->clockmult;
			//LOG_SOUND(("POKEY '%s' chan2 %d\n", p->device->tag(), new_val));
		}

		p->volume[CHAN2] = (p->AUDC[CHAN2] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN2] = new_val;
		if( new_val < p->counter[CHAN2] )
			p->counter[CHAN2] = new_val;
		//if( p->interrupt_cb && p->timer[TIMER2] )
		//	p->timer[TIMER2]->adjust(p->clock_period * new_val, p->timer_param[TIMER2], p->timer_period[TIMER2]);
		p->audible[CHAN2] = !(
			(p->AUDC[CHAN2] & VOLUME_ONLY) ||
			(p->AUDC[CHAN2] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN2] & PURE) && new_val < (p->samplerate_24_8 >> 8)));
		if( !p->audible[CHAN2] )
		{
			p->output[CHAN2] = 1;
			p->counter[CHAN2] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN2] >>= 1;
		}
	}

	if( ch_mask & (1 << CHAN3) )
	{
		/* process channel 3 frequency */
		if( p->AUDCTL & CH3_HICLK )
			new_val = p->AUDF[CHAN3] + DIVADD_HICLK;
		else
			new_val = (p->AUDF[CHAN3] + DIVADD_LOCLK) * p->clockmult;

		//LOG_SOUND(("POKEY '%s' chan3 %d\n", p->device->tag(), new_val));

		p->volume[CHAN3] = (p->AUDC[CHAN3] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN3] = new_val;
		if( new_val < p->counter[CHAN3] )
			p->counter[CHAN3] = new_val;
		/* channel 3 does not have a timer associated */
		p->audible[CHAN3] = !(
			(p->AUDC[CHAN3] & VOLUME_ONLY) ||
			(p->AUDC[CHAN3] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN3] & PURE) && new_val < (p->samplerate_24_8 >> 8))) ||
			(p->AUDCTL & CH1_FILTER);
		if( !p->audible[CHAN3] )
		{
			p->output[CHAN3] = 1;
			p->counter[CHAN3] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN3] >>= 1;
		}
	}

	if( ch_mask & (1 << CHAN4) )
	{
		/* process channel 4 frequency */
		if( p->AUDCTL & CH34_JOINED )
		{
			if( p->AUDCTL & CH3_HICLK )
				new_val = p->AUDF[CHAN4] * 256 + p->AUDF[CHAN3] + DIVADD_HICLK_JOINED;
			else
				new_val = (p->AUDF[CHAN4] * 256 + p->AUDF[CHAN3] + DIVADD_LOCLK) * p->clockmult;
			//LOG_SOUND(("POKEY '%s' chan3+4 %d\n", p->device->tag(), new_val));
		}
		else
		{
			new_val = (p->AUDF[CHAN4] + DIVADD_LOCLK) * p->clockmult;
			//LOG_SOUND(("POKEY '%s' chan4 %d\n", p->device->tag(), new_val));
		}

		p->volume[CHAN4] = (p->AUDC[CHAN4] & VOLUME_MASK) * POKEY_DEFAULT_GAIN;
		p->divisor[CHAN4] = new_val;
		if( new_val < p->counter[CHAN4] )
			p->counter[CHAN4] = new_val;
		//if( p->interrupt_cb && p->timer[TIMER4] )
		//	p->timer[TIMER4]->adjust(p->clock_period * new_val, p->timer_param[TIMER4], p->timer_period[TIMER4]);
		p->audible[CHAN4] = !(
			(p->AUDC[CHAN4] & VOLUME_ONLY) ||
			(p->AUDC[CHAN4] & VOLUME_MASK) == 0 ||
			((p->AUDC[CHAN4] & PURE) && new_val < (p->samplerate_24_8 >> 8))) ||
			(p->AUDCTL & CH2_FILTER);
		if( !p->audible[CHAN4] )
		{
			p->output[CHAN4] = 1;
			p->counter[CHAN4] = 0x7fffffff;
			/* 50% duty cycle should result in half volume */
			p->volume[CHAN4] >>= 1;
		}
	}
}


static void pokey_set_mute_mask(void *info, UINT32 MuteMask)
{
	pokey_state *chip = (pokey_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
		chip->Muted[CurChn] = (MuteMask >> CurChn) & 0x01;
	
	return;
}
