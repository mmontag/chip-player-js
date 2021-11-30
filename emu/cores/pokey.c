// license:BSD-3-Clause
// copyright-holders:Brad Oliver, Eric Smith, Juergen Buchmueller
/*****************************************************************************
 *
 *  POKEY chip emulator 4.6
 *
 *  Based on original info found in Ron Fries' Pokey emulator,
 *  with additions by Brad Oliver, Eric Smith and Juergen Buchmueller,
 *  paddle (a/d conversion) details from the Atari 400/800 Hardware Manual.
 *  Polynomial algorithms according to info supplied by Perry McFarlane.
 *
 *  4.6:
 *    [1] http://ploguechipsounds.blogspot.de/2009/10/how-i-recorded-and-decoded-pokeys.html
 *  - changed audio emulation to emulate borrow 3 clock delay and
 *    proper channel reset. New frequency only becomes effective
 *    after the counter hits 0. Emulation also treats counters
 *    as 8 bit counters which are linked now instead of monolithic
 *    16 bit counters.
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
 *    polynomial shifters to 0) returns the expected 0xff value.
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
 *  TODO:  liberatr clipping
 *  TODO:  bit-level serial I/O instead of fake byte read/write handlers
 *
 *
 *****************************************************************************/

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "pokey.h"

typedef struct pokey_device pokey_device;
typedef struct pokey_channel pokey_channel;

static void pokey_device_update(void *, UINT32 samples, DEV_SMPL **outputs);
static UINT8 pokey_device_read(pokey_device *, UINT8 offset);
static void pokey_device_write(pokey_device *, UINT8 offset, UINT8 data);
static UINT8 pokey_device_start(const DEV_GEN_CFG *, DEV_INFO *);
static void pokey_device_stop(void *);
static void pokey_device_reset(void *);
static void pokey_device_set_mute_mask(void *, UINT32 mutes);

static void pokey_device_step_one_clock(pokey_device *);
INLINE void pokey_device_process_channel(pokey_device *d, int ch);

static void poly_init_4_5(UINT32 *poly, int size, int xorbit, int invert);
static void poly_init_9_17(UINT32 *poly, int size);
static void pokey_device_vol_init(pokey_device *d);
static void pokey_device_potgo(pokey_device *d);

static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, pokey_device_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, pokey_device_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, pokey_device_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"POKEY", "MAME", FCC_MAME,
	
	pokey_device_start,
	pokey_device_stop,
	pokey_device_reset,
	pokey_device_update,
	
	NULL,	// SetOptionBits
	pokey_device_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_Pokey[] =
{
	&devDef,
	NULL
};

/* constants, structs etc from MAME's original pokey.h, with c++ converted to C as appropriate */

/* CONSTANT DEFINITIONS */

/* exact 1.79 MHz clock freq (of the Atari 800 that is) */
/* static constexpr unsigned FREQ_17_EXACT = 1789790; */
#define FREQ_17_EXACT 1789790

/* static constexpr int POKEY_CHANNELS = 4; */
#define POKEY_CHANNELS 4

enum
{
	POK_KEY_BREAK = 0x30,
	POK_KEY_SHIFT = 0x20,
	POK_KEY_CTRL  = 0x00
};

enum
{
	/* POKEY WRITE LOGICALS */
	AUDF1_C  =   0x00,
	AUDC1_C  =   0x01,
	AUDF2_C  =   0x02,
	AUDC2_C  =   0x03,
	AUDF3_C  =   0x04,
	AUDC3_C  =   0x05,
	AUDF4_C  =   0x06,
	AUDC4_C  =   0x07,
	AUDCTL_C =   0x08,
	STIMER_C =   0x09,
	SKREST_C =   0x0A,
	POTGO_C  =   0x0B,
	SEROUT_C =   0x0D,
	IRQEN_C  =   0x0E,
	SKCTL_C  =   0x0F
};

enum
{
	/* POKEY READ LOGICALS */
	POT0_C   =  0x00,
	POT1_C   =  0x01,
	POT2_C   =  0x02,
	POT3_C   =  0x03,
	POT4_C   =  0x04,
	POT5_C   =  0x05,
	POT6_C   =  0x06,
	POT7_C   =  0x07,
	ALLPOT_C =  0x08,
	KBCODE_C =  0x09,
	RANDOM_C =  0x0A,
	SERIN_C  =  0x0D,
	IRQST_C  =  0x0E,
	SKSTAT_C =  0x0F
};

enum /* sync-operations */
{
	SYNC_NOOP	   = 11,
	SYNC_SET_IRQST  = 12,
	SYNC_POT		= 13,
	SYNC_WRITE	  = 14
};

enum output_type
{
	LEGACY_LINEAR = 0,
	RC_LOWPASS,
	OPAMP_C_TO_GROUND,
	OPAMP_LOW_PASS,
	DISCRETE_VAR_R
};

/* pokey_channel class, as a struct */

struct pokey_channel
{
	pokey_device *m_parent;
	UINT8 m_INTMask;
	UINT8 m_AUDF;           /* AUDFx (D200, D202, D204, D206) */
	UINT8 m_AUDC;           /* AUDCx (D201, D203, D205, D207) */
	INT32 m_borrow_cnt;     /* borrow counter */
	INT32 m_counter;        /* channel counter */
	UINT8 m_output;         /* channel output signal (1 active, 0 inactive) */
	UINT8 m_filter_sample;  /* high-pass filter sample */
};

struct pokey_device
{
	DEV_DATA _devData;
	UINT8 m_muted[POKEY_CHANNELS];

	pokey_channel m_channel[POKEY_CHANNELS];
	UINT32 m_out_raw;         /* raw output */
	UINT8 m_old_raw_inval;    /* true: recalc m_out_raw required */
	double m_out_filter;      /* filtered output */

	INT32 m_clock_cnt[3];	  /* clock counters */
	UINT32 m_p4;			  /* poly4 index */
	UINT32 m_p5;			  /* poly5 index */
	UINT32 m_p9;			  /* poly9 index */
	UINT32 m_p17;			  /* poly17 index */

	UINT8 m_POTx[8];		  /* POTx   (R/D200-D207) */
	UINT8 m_AUDCTL;		      /* AUDCTL (W/D208) */
	UINT8 m_ALLPOT;		      /* ALLPOT (R/D208) */
	UINT8 m_KBCODE;		      /* KBCODE (R/D209) */
	UINT8 m_SERIN;		      /* SERIN  (R/D20D) */
	UINT8 m_SEROUT;		      /* SEROUT (W/D20D) */
	UINT8 m_IRQST;		      /* IRQST  (R/D20E) */
	UINT8 m_IRQEN;		      /* IRQEN  (W/D20E) */
	UINT8 m_SKSTAT;		      /* SKSTAT (R/D20F) */
	UINT8 m_SKCTL;		      /* SKCTL  (W/D20F) */

	UINT8 m_pot_counter;
	UINT8 m_kbd_cnt;
	UINT8 m_kbd_latch;
	UINT8 m_kbd_state;

	double m_clock_period;

	UINT32 m_poly4[0x0f];
	UINT32 m_poly5[0x1f];
	UINT32 m_poly9[0x1ff];
	UINT32 m_poly17[0x1ffff];

	double m_voltab[0x10000];

	enum output_type m_output_type;
	double m_r_pullup;
	double m_cap;
	double m_v_ref;

	int m_icount;
};

INLINE void pokey_channel_sample(pokey_channel *c)
{
	c->m_filter_sample = c->m_output;
}

INLINE void pokey_channel_reset_channel(pokey_channel *c)
{
	c->m_counter = c->m_AUDF ^ 0xff;
}

INLINE void pokey_channel_inc_chan(pokey_channel *c)
{
	c->m_counter = (c->m_counter + 1) & 0xff;
	if (c->m_counter == 0 && c->m_borrow_cnt == 0)
	{
		c->m_borrow_cnt = /*3*/1;	/* immediate trigger = partly fix BMX Simulator */
		if (c->m_parent->m_IRQEN & c->m_INTMask)
		{
			/* Exposed state has changed: This should only be updated after a resync ... */
			// c->m_parent->synchronize(SYNC_SET_IRQST, c->m_INTMask);
		}
	}
}

INLINE int pokey_channel_check_borrow(pokey_channel *c)
{
	if (c->m_borrow_cnt > 0)
	{
		c->m_borrow_cnt--;
		return (c->m_borrow_cnt == 0);
	}
	return 0;
}


static void pokey_device_set_output_rc(pokey_device *d, double r, double c, double v)
{
	d->m_output_type = RC_LOWPASS;
	d->m_r_pullup = r;
	d->m_cap = c;
	d->m_v_ref = v;
}

static void pokey_device_set_output_opamp(pokey_device *d, double r, double c, double v)
{
	d->m_output_type = OPAMP_C_TO_GROUND;
	d->m_r_pullup = r;
	d->m_cap = c;
	d->m_v_ref = v;
}

static void pokey_device_set_output_opamp_low_pass(pokey_device *d, double r, double c, double v)
{
	d->m_output_type = OPAMP_LOW_PASS;
	d->m_r_pullup = r;
	d->m_cap = c;
	d->m_v_ref = v;
}

static void set_output_discrete(pokey_device *d)
{
	d->m_output_type = DISCRETE_VAR_R;
}
/* end mame's original pokey.h */

/* Four channels with a range of 0..32767 and volume 0..15 */
//#define POKEY_DEFAULT_GAIN (32767/15/4)

/*
 * But we raise the gain and risk clipping, the old Pokey did
 * this too. It defined POKEY_DEFAULT_GAIN 6 and this was
 * 6 * 15 * 4 = 360, 360/256 = 1.40625
 * I use 15/11 = 1.3636, so this is a little lower.
 */

#define POKEY_DEFAULT_GAIN (32767/11/4)

#define CHAN1   0
#define CHAN2   1
#define CHAN3   2
#define CHAN4   3

#define TIMER1  0
#define TIMER2  1
#define TIMER4  2

/* AUDCx */
#define NOTPOLY5    0x80    /* selects POLY5 or direct CLOCK */
#define POLY4       0x40    /* selects POLY4 or POLY17 */
#define PURE        0x20    /* selects POLY4/17 or PURE tone */
#define VOLUME_ONLY 0x10    /* selects VOLUME OUTPUT ONLY */
#define VOLUME_MASK 0x0f    /* volume mask */

/* AUDCTL */
#define POLY9       0x80    /* selects POLY9 or POLY17 */
#define CH1_HICLK   0x40    /* selects 1.78979 MHz for Ch 1 */
#define CH3_HICLK   0x20    /* selects 1.78979 MHz for Ch 3 */
#define CH12_JOINED 0x10    /* clocks channel 1 w/channel 2 */
#define CH34_JOINED 0x08    /* clocks channel 3 w/channel 4 */
#define CH1_FILTER  0x04    /* selects channel 1 high pass filter */
#define CH2_FILTER  0x02    /* selects channel 2 high pass filter */
#define CLK_15KHZ   0x01    /* selects 15.6999 kHz or 63.9211 kHz */

/* IRQEN (D20E) */
#define IRQ_BREAK   0x80    /* BREAK key pressed interrupt */
#define IRQ_KEYBD   0x40    /* keyboard data ready interrupt */
#define IRQ_SERIN   0x20    /* serial input data ready interrupt */
#define IRQ_SEROR   0x10    /* serial output register ready interrupt */
#define IRQ_SEROC   0x08    /* serial output complete interrupt */
#define IRQ_TIMR4   0x04    /* timer channel #4 interrupt */
#define IRQ_TIMR2   0x02    /* timer channel #2 interrupt */
#define IRQ_TIMR1   0x01    /* timer channel #1 interrupt */

/* SKSTAT (R/D20F) */
#define SK_FRAME    0x80    /* serial framing error */
#define SK_KBERR    0x40    /* keyboard overrun error - pokey documentation states *some bit as IRQST */
#define SK_OVERRUN  0x20    /* serial overrun error - pokey documentation states *some bit as IRQST */
#define SK_SERIN    0x10    /* serial input high */
#define SK_SHIFT    0x08    /* shift key pressed */
#define SK_KEYBD    0x04    /* keyboard key pressed */
#define SK_SEROUT   0x02    /* serial output active */

/* SKCTL (W/D20F) */
#define SK_BREAK    0x80    /* serial out break signal */
#define SK_BPS      0x70    /* bits per second */
#define SK_FM       0x08    /* FM mode */
#define SK_PADDLE   0x04    /* fast paddle a/d conversion */
#define SK_RESET    0x03    /* reset serial/keyboard interface */
#define SK_KEYSCAN  0x02    /* key scanning enabled ? */
#define SK_DEBOUNCE 0x01    /* Debouncing ?*/

#define DIV_64      28       /* divisor for 1.78979 MHz clock to 63.9211 kHz */
#define DIV_15      114      /* divisor for 1.78979 MHz clock to 15.6999 kHz */

#define CLK_1 0
#define CLK_28 1
#define CLK_114 2

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************
static void pokey_device_set_mute_mask(void *info, UINT32 mask)
{
	pokey_device *d = (pokey_device *)info;
	UINT8 ch;
	for(ch = 0; ch < POKEY_CHANNELS; ch++) {
		d->m_muted[ch] = (mask >> ch) & 0x01;
	}
}

static void pokey_device_stop(void *info)
{
	pokey_device *d = (pokey_device *)info;
	free(d);
}

static UINT8 pokey_device_start(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	pokey_device *d = NULL;

	d = (pokey_device *)calloc(1,sizeof(pokey_device));
	if(d == NULL) return 0xFF;

	d->m_clock_period = 1.0 / cfg->clock;
	d->m_output_type = LEGACY_LINEAR;

	// bind callbacks
	/*
	m_keyboard_r.resolve();
	m_irq_f.resolve();
	*/

	/* calculate the A/D times
	 * In normal, slow mode (SKCTL bit SK_PADDLE is clear) the conversion
	 * takes N scanlines, where N is the paddle value. A single scanline
	 * takes approximately 64us to finish (1.78979MHz clock).
	 * In quick mode (SK_PADDLE set) the conversion is done very fast
	 * (takes two scanlines) but the result is not as accurate.
	 */

	/* initialize the poly counters */
	poly_init_4_5(d->m_poly4, 4, 1, 0);
	poly_init_4_5(d->m_poly5, 5, 2, 1);

	/* initialize 9 / 17 arrays */
	poly_init_9_17(d->m_poly9,   9);
	poly_init_9_17(d->m_poly17, 17);
	pokey_device_vol_init(d);

	pokey_device_set_mute_mask(d, 0x00);

	INIT_DEVINF(retDevInf, &d->_devData, cfg->clock, &devDef);
	return 0x00;
}

static void pokey_device_reset(void *info)
{
	pokey_device *d = (pokey_device *)info;
	int i;

	/* Setup channels */
	for (i=0; i<POKEY_CHANNELS; i++)
	{
		d->m_channel[i].m_parent = d;
		d->m_channel[i].m_INTMask = 0;
		d->m_channel[i].m_AUDF = 0;
		d->m_channel[i].m_AUDC = 0;
		d->m_channel[i].m_borrow_cnt = 0;
		d->m_channel[i].m_counter = 0;
		d->m_channel[i].m_output = 0;
		d->m_channel[i].m_filter_sample = 0;
	}
	d->m_channel[CHAN1].m_INTMask = IRQ_TIMR1;
	d->m_channel[CHAN2].m_INTMask = IRQ_TIMR2;
	d->m_channel[CHAN4].m_INTMask = IRQ_TIMR4;

	/* The pokey does not have a reset line. These should be initialized
	 * with random values.
	 */

	d->m_KBCODE = 0x09;		 /* Atari 800 'no key' */
	d->m_SKCTL = SK_RESET;  /* let the RNG run after reset */
	d->m_SKSTAT = 0;
	/* This bit should probably get set later. Acid5200 pokey_setoc test tests this. */
	d->m_IRQST = IRQ_SEROC;
	d->m_IRQEN = 0;
	d->m_AUDCTL = 0;
	d->m_p4 = 0;
	d->m_p5 = 0;
	d->m_p9 = 0;
	d->m_p17 = 0;
	d->m_ALLPOT = 0x00;

	d->m_pot_counter = 0;
	d->m_kbd_cnt = 0;
	d->m_out_filter = 0;
	d->m_out_raw = 0;
	d->m_old_raw_inval = 1;
	d->m_kbd_state = 0;

	d->m_icount = 0;

	/* reset more internal state */
	memset(d->m_clock_cnt,0,sizeof(d->m_clock_cnt));
	memset(d->m_POTx,0,sizeof(d->m_POTx));

}

//-------------------------------------------------
//  stream_generate - handle update requests for
//  our sound stream
//-------------------------------------------------


static void pokey_device_execute_run(pokey_device *d)
{
	do
	{
		pokey_device_step_one_clock(d);
		d->m_icount--;
	} while (d->m_icount > 0);

}

/*
 * http://www.atariage.com/forums/topic/3328-sio-protocol/page__st__100#entry1680190:
 * I noticed that the Pokey counters have clocked carry (actually, "borrow") positions that delay the
 * counter by 3 cycles, plus the 1 reset clock. So 16 bit mode has 6 carry delays and a reset clock.
 * I'm sure this was done because the propagation delays limited the number of cells the subtraction could ripple though.
 *
 */

static void pokey_device_step_one_clock(pokey_device *d)
{
	/* Clocks only count if we are not in a reset */
	if (d->m_SKCTL & SK_RESET)
	{
		int clock_triggered[3] = {1,0,0};
		int const base_clock = (d->m_AUDCTL & CLK_15KHZ) ? CLK_114 : CLK_28;
		int clk;

		/* polynom pointers */
		if (++d->m_p4 == 0x0000f)
			d->m_p4 = 0;
		if (++d->m_p5 == 0x0001f)
			d->m_p5 = 0;
		if (++d->m_p9 == 0x001ff)
			d->m_p9 = 0;
		if (++d->m_p17 == 0x1ffff)
			d->m_p17 = 0;

		/* CLK_1: no presacler */
		//int clock_triggered[3] = {1,0,0};
		/* CLK_28: prescaler 63.9211 kHz */
		if (++d->m_clock_cnt[CLK_28] >= DIV_64)
		{
			d->m_clock_cnt[CLK_28] = 0;
			clock_triggered[CLK_28] = 1;
		}
		/* CLK_114 prescaler 15.6999 kHz */
		if (++d->m_clock_cnt[CLK_114] >= DIV_15)
		{
			d->m_clock_cnt[CLK_114] = 0;
			clock_triggered[CLK_114] = 1;
		}

		//int const base_clock = (d->m_AUDCTL & CLK_15KHZ) ? CLK_114 : CLK_28;
		clk = (d->m_AUDCTL & CH1_HICLK) ? CLK_1 : base_clock;
		if (clock_triggered[clk])
			pokey_channel_inc_chan(&d->m_channel[CHAN1]);

		clk = (d->m_AUDCTL & CH3_HICLK) ? CLK_1 : base_clock;
		if (clock_triggered[clk])
			pokey_channel_inc_chan(&d->m_channel[CHAN3]);

		if (clock_triggered[base_clock])
		{
			if (!(d->m_AUDCTL & CH12_JOINED))
				pokey_channel_inc_chan(&d->m_channel[CHAN2]);
			if (!(d->m_AUDCTL & CH34_JOINED))
				pokey_channel_inc_chan(&d->m_channel[CHAN4]);
		}
	}

	if (pokey_channel_check_borrow(&d->m_channel[CHAN1]))
	{
		if (d->m_AUDCTL & CH12_JOINED)
			pokey_channel_inc_chan(&d->m_channel[CHAN2]);
		else
			pokey_channel_reset_channel(&d->m_channel[CHAN1]);
		pokey_device_process_channel(d,CHAN1);
	}

	/* do CHAN2 before CHAN1 because CHAN1 may set borrow! */
	/* NOT doing it + borrow = 1 cycle = partly fix BMX Simulator (same for CHAN3/4) */
	if (pokey_channel_check_borrow(&d->m_channel[CHAN2]))
	{
		if (d->m_AUDCTL & CH12_JOINED)
			pokey_channel_reset_channel(&d->m_channel[CHAN1]);
		pokey_channel_reset_channel(&d->m_channel[CHAN2]);
		pokey_device_process_channel(d,CHAN2);
	}

	if (pokey_channel_check_borrow(&d->m_channel[CHAN3]))
	{
		if (d->m_AUDCTL & CH34_JOINED)
			pokey_channel_inc_chan(&d->m_channel[CHAN4]);
		else
			pokey_channel_reset_channel(&d->m_channel[CHAN3]);
		pokey_device_process_channel(d,CHAN3);
		/* is this a filtering channel (3/4) and is the filter active? */
		if (d->m_AUDCTL & CH1_FILTER)
			pokey_channel_sample(&d->m_channel[CHAN1]);
		else
			d->m_channel[CHAN1].m_filter_sample = 1;
	}

	/* do CHAN4 before CHAN3 because CHAN3 may set borrow! */
	if (pokey_channel_check_borrow(&d->m_channel[CHAN4]))
	{
		if (d->m_AUDCTL & CH34_JOINED)
			pokey_channel_reset_channel(&d->m_channel[CHAN3]);
		pokey_channel_reset_channel(&d->m_channel[CHAN4]);
		pokey_device_process_channel(d,CHAN4);
		/* is this a filtering channel (3/4) and is the filter active? */
		if (d->m_AUDCTL & CH2_FILTER)
			pokey_channel_sample(&d->m_channel[CHAN2]);
		else
			d->m_channel[CHAN2].m_filter_sample = 1;
	}

	if (d->m_old_raw_inval)
	{
		UINT32 sum = 0;
		int ch;
		for (ch = 0; ch < 4; ch++)
		{
			if(!d->m_muted[ch]) {
				sum |= (((d->m_channel[ch].m_output ^ d->m_channel[ch].m_filter_sample) || (d->m_channel[ch].m_AUDC & VOLUME_ONLY)) ?
					((d->m_channel[ch].m_AUDC & VOLUME_MASK) << (ch * 4)) : 0);
			}
		}

		d->m_old_raw_inval = 0;
		d->m_out_raw = sum;
	}
}

//-------------------------------------------------
//  sound_stream_update - handle update requests for
//  our sound stream
//-------------------------------------------------

void pokey_device_update(void *info, UINT32 samples, DEV_SMPL **outputs)
{
	pokey_device *d = (pokey_device *)info;
	UINT32 sampindex;

	for(sampindex = 0; sampindex < samples; sampindex++) {
		pokey_device_execute_run(d);

		if (d->m_output_type == LEGACY_LINEAR)
		{
			INT32 out = 0;
			int i;
			for (i = 0; i < 4; i++)
				out += ((d->m_out_raw >> (4*i)) & 0x0f);
			out *= POKEY_DEFAULT_GAIN;
			out = (out > 0x7fff) ? 0x7fff : out;
			outputs[0][sampindex] = out;
			outputs[1][sampindex] = out;
		}
		else if (d->m_output_type == RC_LOWPASS)
		{
			double rTot = d->m_voltab[d->m_out_raw];

			double V0 = rTot / (rTot+d->m_r_pullup) * d->m_v_ref / 5.0;
			double mult = (d->m_cap == 0.0) ? 1.0 : 1.0 - exp(-(rTot + d->m_r_pullup) / (d->m_cap * d->m_r_pullup * rTot) * d->m_clock_period);

			/* store sum of output signals into the buffer */
			d->m_out_filter += (V0 - d->m_out_filter) * mult;

            /* TODO verify that d->m_out_filter is in the range -1.0 - +1.0 */
			outputs[0][sampindex] = d->m_out_filter * 0x7fff;
			outputs[1][sampindex] = d->m_out_filter * 0x7fff;
		}
		else if (d->m_output_type == OPAMP_C_TO_GROUND)
		{
			double rTot = d->m_voltab[d->m_out_raw];
			/* In this configuration there is a capacitor in parallel to the pokey output to ground.
			 * With a LM324 in LTSpice this causes the opamp circuit to oscillate at around 100 kHz.
			 * We are ignoring the capacitor here, since this oscillation would not be audible.
			 */

			/* This post-pokey stage usually has a high-pass filter behind it
			 * It is approximated by eliminating m_v_ref ( -1.0 term)
			 */

			double V0 = ((rTot+d->m_r_pullup) / rTot - 1.0) * d->m_v_ref  / 5.0;
	   		/* store sum of output signals into the buffer */

            /* TODO verify that V0 is in the range -1.0 - +1.0 */
			outputs[0][sampindex] = V0 * 0x7fff;
			outputs[1][sampindex] = V0 * 0x7fff;
		}
		else if (d->m_output_type == OPAMP_LOW_PASS)
		{
			double rTot = d->m_voltab[d->m_out_raw];
			/* This post-pokey stage usually has a low-pass filter behind it
			 * It is approximated by not adding in VRef below.
			 */

			double V0 = (d->m_r_pullup / rTot) * d->m_v_ref  / 5.0;
			double mult = (d->m_cap == 0.0) ? 1.0 : 1.0 - exp(-1.0 / (d->m_cap * d->m_r_pullup) * d->m_clock_period);

			/* store sum of output signals into the buffer */
			d->m_out_filter += (V0 - d->m_out_filter) * mult;

            /* TODO verify that d->m_out_filter is in the range -1.0 - +1.0 */
			outputs[0][sampindex] = d->m_out_filter * 0x7fff;
			outputs[1][sampindex] = d->m_out_filter * 0x7fff;
		}
		else if (d->m_output_type == DISCRETE_VAR_R)
		{
			/* store sum of output signals into the buffer */

            /* TODO verify that d->m_voltab is in the range -1.0 - +1.0 */
			outputs[0][sampindex] = d->m_voltab[d->m_out_raw] * 0x7fff;
			outputs[1][sampindex] = d->m_voltab[d->m_out_raw] * 0x7fff;
		}
	}
}

//-------------------------------------------------
//  read - memory interface for reading the active status
//-------------------------------------------------

UINT8 pokey_device_read(pokey_device *d, UINT8 offset)
{
	int data, pot;

	switch (offset & 15)
	{
	case POT0_C: case POT1_C: case POT2_C: case POT3_C:
	case POT4_C: case POT5_C: case POT6_C: case POT7_C:
		pot = offset & 7;
		if (d->m_ALLPOT & (1 << pot))
		{
			/* we have a value measured */
			data = d->m_POTx[pot];
		}
		else
		{
			data = d->m_pot_counter;
		}
		break;

	case ALLPOT_C:
		/****************************************************************
		 * If the 2 least significant bits of SKCTL are 0, the ALLPOTs
		 * are disabled (SKRESET). Thanks to MikeJ for pointing this out.
		 ****************************************************************/
		if ((d->m_SKCTL & SK_RESET) == 0)
		{
			data = d->m_ALLPOT;
		}
		else
		{
			data = d->m_ALLPOT ^ 0xff;
		}
		break;

	case KBCODE_C:
		data = d->m_KBCODE;
		break;

	case RANDOM_C:
		if (d->m_AUDCTL & POLY9)
		{
			data = d->m_poly9[d->m_p9] & 0xff;
		}
		else
		{
			data = (d->m_poly17[d->m_p17] >> 8) & 0xff;
		}
		break;

	case SERIN_C:
		data = d->m_SERIN;
		break;

	case IRQST_C:
		/* IRQST is an active low input port; we keep it active high */
		/* internally to ease the (un-)masking of bits */
		data = d->m_IRQST ^ 0xff;
		break;

	case SKSTAT_C:
		/* SKSTAT is also an active low input port */
		data = d->m_SKSTAT ^ 0xff;
		break;

	default:
		data = 0xff;
		break;
	}
	return data;
}


//-------------------------------------------------
//  write - memory interface for write
//-------------------------------------------------

void pokey_device_write(pokey_device *d, UINT8 offset, UINT8 data)
{
	/* determine which address was changed */
	switch (offset & 15)
	{
	case AUDF1_C:
		d->m_channel[CHAN1].m_AUDF = data;
		break;

	case AUDC1_C:
		d->m_channel[CHAN1].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDF2_C:
		d->m_channel[CHAN2].m_AUDF = data;
		break;

	case AUDC2_C:
		d->m_channel[CHAN2].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDF3_C:
		d->m_channel[CHAN3].m_AUDF = data;
		break;

	case AUDC3_C:
		d->m_channel[CHAN3].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDF4_C:
		d->m_channel[CHAN4].m_AUDF = data;
		break;

	case AUDC4_C:
		d->m_channel[CHAN4].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDCTL_C:
		if( data == d->m_AUDCTL )
			return;
		d->m_AUDCTL = data;

		break;

	case STIMER_C:
	{
		/* From the pokey documentation:
		 * reset all counters to zero (side effect)
		 * Actually this takes 4 cycles to actually happen.
		 * FIXME: Use timer for delayed reset !
		 */
		int i;
		for (i = 0; i < POKEY_CHANNELS; i++)
		{
			pokey_channel_reset_channel(&d->m_channel[i]);
			d->m_channel[i].m_output = 0;
			d->m_channel[i].m_filter_sample = (i<2 ? 1 : 0);
		}
		d->m_old_raw_inval = 1;
		break;
	}

	case SKREST_C:
		/* reset SKSTAT */
		d->m_SKSTAT &= ~(SK_FRAME|SK_OVERRUN|SK_KBERR);
		break;

	case POTGO_C:
		pokey_device_potgo(d);
		break;

	case SEROUT_C:
		d->m_SKSTAT |= SK_SEROUT;
		break;

	case IRQEN_C:

		/* acknowledge one or more IRQST bits ? */
		if( d->m_IRQST & ~data )
		{
			/* reset IRQST bits that are masked now, except the SEROC bit (acid5200 pokey_seroc test) */
			d->m_IRQST &= (IRQ_SEROC | data);
		}
		/* store irq enable */
		d->m_IRQEN = data;
		break;

	case SKCTL_C:
		if( data == d->m_SKCTL )
			return;
		d->m_SKCTL = data;
		if( !(data & SK_RESET) )
		{
			pokey_device_write(d,IRQEN_C,  0);
			pokey_device_write(d,SKREST_C, 0);
			/****************************************************************
			 * If the 2 least significant bits of SKCTL are 0, the random
			 * number generator is disabled (SKRESET). Thanks to Eric Smith
			 * for pointing out this critical bit of info!
			 * Couriersud: Actually, the 17bit poly is reset and kept in a
			 * reset state.
			 ****************************************************************/
			d->m_p9 = 0;
			d->m_p17 = 0;
			d->m_p4 = 0;
			d->m_p5 = 0;
			d->m_clock_cnt[0] = 0;
			d->m_clock_cnt[1] = 0;
			d->m_clock_cnt[2] = 0;
			d->m_old_raw_inval = 1;
			/* FIXME: Serial port reset ! */
		}
		break;
	}

	/************************************************************
	 * As defined in the manual, the exact counter values are
	 * different depending on the frequency and resolution:
	 *	64 kHz or 15 kHz - AUDF + 1
	 *	1.79 MHz, 8-bit  - AUDF + 4
	 *	1.79 MHz, 16-bit - AUDF[CHAN1]+256*AUDF[CHAN2] + 7
	 ************************************************************/

}


//-------------------------------------------------
//  private stuff
//-------------------------------------------------

INLINE void pokey_device_process_channel(pokey_device *d, int ch)
{
	if ((d->m_channel[ch].m_AUDC & NOTPOLY5) || (d->m_poly5[d->m_p5] & 1))
	{
		if (d->m_channel[ch].m_AUDC & PURE)
			d->m_channel[ch].m_output ^= 1;
		else if (d->m_channel[ch].m_AUDC & POLY4)
			d->m_channel[ch].m_output = (d->m_poly4[d->m_p4] & 1);
		else if (d->m_AUDCTL & POLY9)
			d->m_channel[ch].m_output = (d->m_poly9[d->m_p9] & 1);
		else
			d->m_channel[ch].m_output = (d->m_poly17[d->m_p17] & 1);
		d->m_old_raw_inval = 1;
	}
}


static void pokey_device_potgo(pokey_device *d)
{
	int pot;

	if( (d->m_SKCTL & SK_RESET) == 0)
		return;

	d->m_ALLPOT = 0x00;
	d->m_pot_counter = 0;

	for( pot = 0; pot < 8; pot++ )
	{
		d->m_POTx[pot] = 228;
	}
}

static void pokey_device_vol_init(pokey_device *d)
{
	double resistors[4] = {90000, 26500, 8050, 3400};
	double pull_up = 10000;
	/* just a guess, there has to be a resistance since the doc specifies that
	 * Vout is at least 4.2V if all channels turned off.
	 */
	double r_off = 8e6;
	double r_chan[16];
	double rTot;
	int i, j;

	for (j=0; j<16; j++)
	{
		rTot = 1.0 / 1e12; /* avoid div by 0 */;
		for (i=0; i<4; i++)
		{
			if (j & (1 << i))
				rTot += 1.0 / resistors[i];
			else
				rTot += 1.0 / r_off;
		}
		r_chan[j] = 1.0 / rTot;
	}

	for (j=0; j<0x10000; j++)
	{
		rTot = 0;
		for (i=0; i<4; i++)
		{
			rTot += 1.0 / r_chan[(j >> (i*4)) & 0x0f];
		}
		rTot = 1.0 / rTot;
		d->m_voltab[j] = rTot;
	}

}

static void poly_init_4_5(UINT32 *poly, int size, int xorbit, int invert)
{
	int mask = (1 << size) - 1;
	int i;
	UINT32 lfsr = 0;

	for( i = 0; i < mask; i++ )
	{
		/* calculate next bit */
		int in = !((lfsr >> 0) & 1) ^ ((lfsr >> xorbit) & 1);
		lfsr = lfsr >> 1;
		lfsr = (in << (size-1)) | lfsr;
		*poly = lfsr ^ invert;
		poly++;
	}
}

static void poly_init_9_17(UINT32 *poly, int size)
{
	int mask = (1 << size) - 1;
	int i;
	UINT32 lfsr =mask;

	if (size == 17)
	{
		for( i = 0; i < mask; i++ )
		{
			/* calculate next bit @ 7 */
			int in8 = ((lfsr >> 8) & 1) ^ ((lfsr >> 13) & 1);
			int in = (lfsr & 1);
			lfsr = lfsr >> 1;
			lfsr = (lfsr & 0xff7f) | (in8 << 7);
			lfsr = (in << 16) | lfsr;
			*poly = lfsr;
			poly++;
		}
	}
	else
	{
		for( i = 0; i < mask; i++ )
		{
			/* calculate next bit */
			int in = ((lfsr >> 0) & 1) ^ ((lfsr >> 5) & 1);
			lfsr = lfsr >> 1;
			lfsr = (in << 8) | lfsr;
			*poly = lfsr;
			poly++;
		}
	}

}

