// license:BSD-3-Clause
// copyright-holders:Brad Oliver, Eric Smith, Juergen Buchmueller
/*****************************************************************************
 *
 *  POKEY chip emulator 4.9
 *
 *  Based on original info found in Ron Fries' Pokey emulator,
 *  with additions by Brad Oliver, Eric Smith and Juergen Buchmueller,
 *  paddle (a/d conversion) details from the Atari 400/800 Hardware Manual.
 *  Polynomial algorithms according to info supplied by Perry McFarlane.
 *  Additional improvements from Mike Saarna's A7800 MAME fork.
 *
 *  4.9:
 *  - Two-tone mode updated for better accuracy.
 *
 *  4.8:
 *  - Poly5 related modes had a pitch shift issue. The poly4/5 init routine
 *    was replaced with one based on Altira's implementation, which resolved
 *    the issue.
 *
 *  4.7:
 *    [1] https://www.virtualdub.org/downloads/Altirra%20Hardware%20Reference%20Manual.pdf
 *  - updated to reflect that borrowing cycle delays only impacts voices
 *    running at 1.79MHz. (+4 cycles unlinked, or +7 cycles linked)
 *    At slower speeds, cycle overhead still occurs, but only affects
 *    the phase of the timer period, not the actual length.
 *  - Initial two-tone support added. Emulation of two-tone is limited to
 *    audio output effects, and doesn't incorporate any of the aspects of
 *    SIO serial transfer.
 *
 *  4.6:
 *    [2] http://ploguechipsounds.blogspot.de/2009/10/how-i-recorded-and-decoded-pokeys.html
 *  - changed audio emulation to emulate borrow 3 clock delay and
 *    proper channel reset. New frequency only becomes effective
 *    after the counter hits 0. Emulation also treats counters
 *    as 8 bit counters which are linked now instead of monolithic
 *    16 bit counters.
 *
 *  4.51:
 *  - changed to use the attotime datatype
 *
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
 *
 *  4.4:
 *  - reversed sample values to make OFF channels produce a zero signal.
 *    actually de-reversed them; don't remember that I reversed them ;-/
 *
 *  4.3:
 *  - for POT inputs returning zero, immediately assert the ALLPOT
 *    bit after POTGO is written, otherwise start trigger timer
 *    depending on SK_PADDLE mode, either 1-228 scanlines or 1-2
 *    scanlines, depending on the SK_PADDLE bit of SKCTL.
 *
 *  4.2:
 *  - half volume for channels which are inaudible (this should be
 *    close to the real thing).
 *
 *  4.1:
 *  - default gain increased to closely match the old code.
 *  - random numbers repeat rate depends on POLY9 flag too!
 *  - verified sound output with many, many Atari 800 games,
 *    including the SUPPRESS_INAUDIBLE optimizations.
 *
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
#include "emutypes.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "pokey.h"

typedef struct pokey_device pokey_device;
typedef struct pokey_channel pokey_channel;

static void pokey_update(void *, UINT32 samples, DEV_SMPL **outputs);
static UINT8 pokey_read(pokey_device *, UINT8 offset);
static void pokey_write(pokey_device *, UINT8 offset, UINT8 data);
static UINT8 device_start_pokey(const DEV_GEN_CFG *, DEV_INFO *);
static void device_stop_pokey(void *);
static void device_reset_pokey(void *);
static void pokey_set_mute_mask(void *, UINT32 mutes);

static void pokey_step_one_clock(pokey_device *);
static void pokey_sid_w(pokey_device *d, UINT8 state);
INLINE void pokey_process_channel(pokey_device *d, int ch);

static void pokey_potgo(pokey_device *d);
static void pokey_vol_init(pokey_device *d);
static void poly_init_4_5(UINT32 *poly, int size);
static void poly_init_9_17(UINT32 *poly, int size);

static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, pokey_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, pokey_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, pokey_set_mute_mask},
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
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_Pokey[] =
{
	&devDef,
	NULL
};

/*
 *  ATARI Pokey (CO12294) pin-out
 *
                 +-----------------+
        VSS      |  1           40 |  D2
        D3       |  2           39 |  D1
        D4       |  3           38 |  D0
        D5       |  4           37 |  AUD
        D6       |  5           36 |  A0
        D7       |  6           35 |  A1
        PHI2     |  7           34 |  A2
        P6       |  8           33 |  A3
        P7       |  9           32 |  R / /W
        P4       | 10           31 |  CS1
        P5       | 11           30 |  /CS0
        P2       | 12           29 |  IRQ
        P3       | 13           28 |  SOD
        P0       | 14           27 |  ACLK
        P1       | 15           26 |  BCLK
        /KR2     | 16           25 |  /KR1
        VCC      | 17           24 |  SID
        /K5      | 18           23 |  /K0
        /K4      | 19           22 |  /K1
        /K3      | 20           21 |  /K2
                 +-----------------+
 *
 */

/* constants, structs etc from MAME's original pokey.h, with c++ converted to C as appropriate */

/* CONSTANT DEFINITIONS */

/* exact 1.79 MHz clock freq (of the Atari 800 that is) */
/* static constexpr unsigned FREQ_17_EXACT = 1789790; */
#define FREQ_17_EXACT 1789790

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
	UINT8 m_AUDF;           // AUDFx (D200, D202, D204, D206)
	UINT8 m_AUDC;           // AUDCx (D201, D203, D205, D207)
	INT32 m_borrow_cnt;     // borrow counter
	INT32 m_counter;        // channel counter
	UINT8 m_output;         // channel output signal (1 active, 0 inactive)
	UINT8 m_filter_sample;  // high-pass filter sample
};

/* static constexpr int POKEY_CHANNELS = 4; */
#define POKEY_CHANNELS 4

struct pokey_device
{
	DEV_DATA _devData;
	UINT8 m_muted[POKEY_CHANNELS];

	pokey_channel m_channel[POKEY_CHANNELS];

	UINT32 m_out_raw;         /* raw output */
	UINT8 m_old_raw_inval;    /* true: recalc m_out_raw required */
	double m_out_filter;      /* filtered output */

	INT32 m_clock_cnt[3];     /* clock counters */
	UINT32 m_p4;              /* poly4 index */
	UINT32 m_p5;              /* poly5 index */
	UINT32 m_p9;              /* poly9 index */
	UINT32 m_p17;             /* poly17 index */

	UINT8 m_POTx[8];          /* POTx   (R/D200-D207) */
	UINT8 m_AUDCTL;           /* AUDCTL (W/D208) */
	UINT8 m_ALLPOT;           /* ALLPOT (R/D208) */
	UINT8 m_KBCODE;           /* KBCODE (R/D209) */
	UINT8 m_SERIN;            /* SERIN  (R/D20D) */
	UINT8 m_SEROUT;           /* SEROUT (W/D20D) */
	UINT8 m_IRQST;            /* IRQST  (R/D20E) */
	UINT8 m_IRQEN;            /* IRQEN  (W/D20E) */
	UINT8 m_SKSTAT;           /* SKSTAT (R/D20F) */
	UINT8 m_SKCTL;            /* SKCTL  (W/D20F) */

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
	c->m_borrow_cnt = 0;
}

INLINE void pokey_channel_inc_chan(pokey_channel *c, pokey_device *host, int cycles)
{
	c->m_counter = (c->m_counter + 1) & 0xff;
	if (c->m_counter == 0 && c->m_borrow_cnt == 0)
	{
		c->m_borrow_cnt = cycles;
		if (c->m_parent->m_IRQEN & c->m_INTMask)
		{
			/* Exposed state has changed: This should only be updated after a resync ... */
			//host.machine().scheduler().synchronize(timer_expired_delegate(pokey_sync_set_irqst, host), c->m_INTMask);
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


static void pokey_set_output_rc(pokey_device *d, double r, double c, double v)
{
	d->m_output_type = RC_LOWPASS;
	d->m_r_pullup = r;
	d->m_cap = c;
	d->m_v_ref = v;
}

static void pokey_set_output_opamp(pokey_device *d, double r, double c, double v)
{
	d->m_output_type = OPAMP_C_TO_GROUND;
	d->m_r_pullup = r;
	d->m_cap = c;
	d->m_v_ref = v;
}

static void pokey_set_output_opamp_low_pass(pokey_device *d, double r, double c, double v)
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
#define SK_TWOTONE  0x08    /* Two tone mode */
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
static void pokey_set_mute_mask(void *info, UINT32 mask)
{
	pokey_device *d = (pokey_device *)info;
	UINT8 ch;
	for(ch = 0; ch < POKEY_CHANNELS; ch++) {
		d->m_muted[ch] = (mask >> ch) & 0x01;
	}
}

static void device_stop_pokey(void *info)
{
	pokey_device *d = (pokey_device *)info;
	free(d);
}

static UINT8 device_start_pokey(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	pokey_device *d = NULL;

	d = (pokey_device *)calloc(1,sizeof(pokey_device));
	if(d == NULL) return 0xFF;

	d->m_clock_period = 1.0 / cfg->clock;
	d->m_output_type = LEGACY_LINEAR;

	// bind callbacks
	//m_keyboard_r.resolve();

	/* calculate the A/D times
	 * In normal, slow mode (SKCTL bit SK_PADDLE is clear) the conversion
	 * takes N scanlines, where N is the paddle value. A single scanline
	 * takes approximately 64us to finish (1.78979MHz clock).
	 * In quick mode (SK_PADDLE set) the conversion is done very fast
	 * (takes two scanlines) but the result is not as accurate.
	 */

	/* initialize the poly counters */
	poly_init_4_5(d->m_poly4, 4);
	poly_init_4_5(d->m_poly5, 5);

	/* initialize 9 / 17 arrays */
	poly_init_9_17(d->m_poly9,   9);
	poly_init_9_17(d->m_poly17, 17);
	pokey_vol_init(d);

	//m_pot_r_cb.resolve_all();
	//m_allpot_r_cb.resolve();
	//m_serin_r_cb.resolve();
	//m_serout_w_cb.resolve_safe();
	//m_irq_w_cb.resolve_safe();

	//m_stream = stream_alloc(0, 1, clock());

	//m_serout_ready_timer = timer_alloc(FUNC(pokey_device::serout_ready_irq), this);
	//m_serout_complete_timer = timer_alloc(FUNC(pokey_device::serout_complete_irq), this);
	//m_serin_ready_timer = timer_alloc(FUNC(pokey_device::serin_ready_irq), this);

	pokey_set_mute_mask(d, 0x00);

	INIT_DEVINF(retDevInf, &d->_devData, cfg->clock, &devDef);
	return 0x00;
}

static void device_reset_pokey(void *info)
{
	pokey_device *d = (pokey_device *)info;
	int i;

	// Set up channels
	for (i=0; i<POKEY_CHANNELS; i++)
	{
		d->m_channel[i].m_parent = d;
		d->m_channel[i].m_INTMask = 0;
		d->m_channel[i].m_AUDF = 0;
		d->m_channel[i].m_AUDC = 0xb0;
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

	d->m_KBCODE = 0x09;		 // Atari 800 'no key'
	//d->m_SKCTL = 0;

	// TODO: several a7800 demos don't explicitly reset pokey at startup
	// See https://atariage.com/forums/topic/337317-a7800-52-release/ and
	// https://atariage.com/forums/topic/268458-a7800-the-atari-7800-emulator/?do=findComment&comment=5079170)
	d->m_SKCTL = SK_RESET;	// NOTE: Some VGMs rely on this being initialized with SK_RESET.

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
	memset(d->m_clock_cnt, 0, sizeof(d->m_clock_cnt));
	memset(d->m_POTx, 0, sizeof(d->m_POTx));

	// a1200xl reads POT4 twice at startup for reading self-test mode jumpers.
	// we need to update POT counters here otherwise it will boot to self-test
	// the first time around no matter the setting.
	pokey_potgo(d);
}

//-------------------------------------------------
//  stream_generate - handle update requests for
//  our sound stream
//-------------------------------------------------


static void pokey_execute_run(pokey_device *d)
{
	do
	{
		pokey_step_one_clock(d);
		d->m_icount--;
	} while (d->m_icount > 0);
}


//-------------------------------------------------
//  step_one_clock - step the whole chip one
//  clock cycle.
//-------------------------------------------------

static void pokey_step_keyboard(pokey_device *d)
{
	if (++d->m_kbd_cnt > 63)
		d->m_kbd_cnt = 0;
	//if (!d->m_keyboard_r.isnull())
	if (0)
	{
		//UINT8 ret = m_keyboard_r(d->m_kbd_cnt);
		UINT8 ret = 0xFF;

		switch (d->m_kbd_cnt)
		{
		case POK_KEY_BREAK:
			if (ret & 2)
			{
				/* check if the break IRQ is enabled */
				if (d->m_IRQEN & IRQ_BREAK)
				{
					//LOG_IRQ("POKEY BREAK IRQ raised\n");
					d->m_IRQST |= IRQ_BREAK;
					//d->m_irq_w_cb(ASSERT_LINE);
				}
			}
			break;
		case POK_KEY_SHIFT:
			d->m_kbd_latch = (d->m_kbd_latch & 0xbf) | ((ret & 2) << 5);
			if (d->m_kbd_latch & 0x40)
				d->m_SKSTAT |= SK_SHIFT;
			else
				d->m_SKSTAT &= ~SK_SHIFT;
			/* FIXME: sync ? */
			break;
		case POK_KEY_CTRL:
			d->m_kbd_latch = (d->m_kbd_latch & 0x7f) | ((ret & 2) << 6);
			break;
		}
		switch (d->m_kbd_state)
		{
		case 0: /* waiting for key */
			if (ret & 1)
			{
				d->m_kbd_latch = (d->m_kbd_latch & 0xc0) | d->m_kbd_cnt;
				d->m_kbd_state++;
			}
			break;
		case 1: /* waiting for key confirmation */
			if (!(d->m_SKCTL & SK_DEBOUNCE) || (d->m_kbd_latch & 0x3f) == d->m_kbd_cnt)
			{
				if (ret & 1)
				{
					d->m_KBCODE = d->m_kbd_latch;
					d->m_SKSTAT |= SK_KEYBD;
					if (d->m_IRQEN & IRQ_KEYBD)
					{
						/* last interrupt not acknowledged ? */
						if (d->m_IRQST & IRQ_KEYBD)
							d->m_SKSTAT |= SK_KBERR;
						//LOG_IRQ("POKEY KEYBD IRQ raised\n");
						d->m_IRQST |= IRQ_KEYBD;
						//d->m_irq_w_cb(ASSERT_LINE);
					}
					d->m_kbd_state++;
				}
				else
					d->m_kbd_state = 0;
			}
			break;
		case 2: /* waiting for release */
			if (!(d->m_SKCTL & SK_DEBOUNCE) || (d->m_kbd_latch & 0x3f) == d->m_kbd_cnt)
			{
				if ((ret & 1)==0)
					d->m_kbd_state++;
			}
			break;
		case 3:
			if (!(d->m_SKCTL & SK_DEBOUNCE) || (d->m_kbd_latch & 0x3f) == d->m_kbd_cnt)
			{
				if (ret & 1)
					d->m_kbd_state = 2;
				else
				{
					d->m_SKSTAT &= ~SK_KEYBD;
					d->m_kbd_state = 0;
				}
			}
			break;
		}
	}
}

static void pokey_step_pot(pokey_device *d)
{
	uint8_t upd = 0;
	int pot;
	d->m_pot_counter++;
	for (pot = 0; pot < 8; pot++)
	{
		if ((d->m_POTx[pot]<d->m_pot_counter) || (d->m_pot_counter == 228))
		{
			upd |= (1 << pot);
			/* latching is emulated in read */
		}
	}
	// some pots latched?
	if (upd != 0)
	{
		//machine().scheduler().synchronize(timer_expired_delegate(FUNC(pokey_device::sync_pot), this), upd);
		d->m_ALLPOT |= upd;
	}
}

/*
 * http://www.atariage.com/forums/topic/3328-sio-protocol/page__st__100#entry1680190:
 * I noticed that the Pokey counters have clocked carry (actually, "borrow") positions that delay the
 * counter by 3 cycles, plus the 1 reset clock. So 16 bit mode has 6 carry delays and a reset clock.
 * I'm sure this was done because the propagation delays limited the number of cells the subtraction could ripple though.
 *
 */

static void pokey_step_one_clock(pokey_device *d)
{
	if (d->m_SKCTL & SK_RESET)
	{
		/* Clocks only count if we are not in a reset */
		int clock_triggered[3] = {1,0,0};
		int base_clock;

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

		if ((d->m_AUDCTL & CH1_HICLK) && (clock_triggered[CLK_1]))
		{
			if (d->m_AUDCTL & CH12_JOINED)
				pokey_channel_inc_chan(&d->m_channel[CHAN1], d, 7);
			else
				pokey_channel_inc_chan(&d->m_channel[CHAN1], d, 4);
		}

		base_clock = (d->m_AUDCTL & CLK_15KHZ) ? CLK_114 : CLK_28;

		if ((!(d->m_AUDCTL & CH1_HICLK)) && (clock_triggered[base_clock]))
			pokey_channel_inc_chan(&d->m_channel[CHAN1], d, 1);

		if ((d->m_AUDCTL & CH3_HICLK) && (clock_triggered[CLK_1]))
		{
			if (d->m_AUDCTL & CH34_JOINED)
				pokey_channel_inc_chan(&d->m_channel[CHAN3], d, 7);
			else
				pokey_channel_inc_chan(&d->m_channel[CHAN3], d, 4);
		}

		if ((!(d->m_AUDCTL & CH3_HICLK)) && (clock_triggered[base_clock]))
			pokey_channel_inc_chan(&d->m_channel[CHAN3], d, 1);

		if (clock_triggered[base_clock])
		{
			if (!(d->m_AUDCTL & CH12_JOINED))
				pokey_channel_inc_chan(&d->m_channel[CHAN2], d, 1);
			if (!(d->m_AUDCTL & CH34_JOINED))
				pokey_channel_inc_chan(&d->m_channel[CHAN4], d, 1);
		}

		/* Potentiometer handling */
		if ((clock_triggered[CLK_114] || (d->m_SKCTL & SK_PADDLE)) && (d->m_pot_counter < 228))
			pokey_step_pot(d);

		/* Keyboard */
		if (clock_triggered[CLK_114] && (d->m_SKCTL & SK_KEYSCAN))
			pokey_step_keyboard(d);
	}

	if (pokey_channel_check_borrow(&d->m_channel[CHAN3]))
	{
		if (d->m_AUDCTL & CH34_JOINED)
			pokey_channel_inc_chan(&d->m_channel[CHAN4], d, 1);
		else
			pokey_channel_reset_channel(&d->m_channel[CHAN3]);

		pokey_process_channel(d, CHAN3);
		/* is this a filtering channel (3/4) and is the filter active? */
		if (d->m_AUDCTL & CH1_FILTER)
			pokey_channel_sample(&d->m_channel[CHAN1]);
		else
			d->m_channel[CHAN1].m_filter_sample = 1;

		d->m_old_raw_inval = true;
	}

	if (pokey_channel_check_borrow(&d->m_channel[CHAN4]))
	{
		if (d->m_AUDCTL & CH34_JOINED)
			pokey_channel_reset_channel(&d->m_channel[CHAN3]);
		pokey_channel_reset_channel(&d->m_channel[CHAN4]);
		pokey_process_channel(d,CHAN4);
		/* is this a filtering channel (3/4) and is the filter active? */
		if (d->m_AUDCTL & CH2_FILTER)
			pokey_channel_sample(&d->m_channel[CHAN2]);
		else
			d->m_channel[CHAN2].m_filter_sample = 1;

		d->m_old_raw_inval = true;
	}

	if ((d->m_SKCTL & SK_TWOTONE) && (d->m_channel[CHAN2].m_borrow_cnt == 1))
	{
		pokey_channel_reset_channel(&d->m_channel[CHAN1]);
		d->m_old_raw_inval = true;
	}

	if (pokey_channel_check_borrow(&d->m_channel[CHAN1]))
	{
		if (d->m_AUDCTL & CH12_JOINED)
			pokey_channel_inc_chan(&d->m_channel[CHAN2], d, 1);
		else
			pokey_channel_reset_channel(&d->m_channel[CHAN1]);

		// TODO: If two-tone is enabled *and* serial output == 1 then reset the channel 2 timer.

		pokey_process_channel(d,CHAN1);
	}

	if (pokey_channel_check_borrow(&d->m_channel[CHAN2]))
	{
		if (d->m_AUDCTL & CH12_JOINED)
			pokey_channel_reset_channel(&d->m_channel[CHAN1]);

		pokey_channel_reset_channel(&d->m_channel[CHAN2]);

		pokey_process_channel(d,CHAN2);
	}

	if (d->m_old_raw_inval)
	{
		UINT32 sum = 0;
		int ch;
		for (ch = 0; ch < 4; ch++)
		{
			if(d->m_muted[ch])
				continue;
			sum |= (((d->m_channel[ch].m_output ^ d->m_channel[ch].m_filter_sample) || (d->m_channel[ch].m_AUDC & VOLUME_ONLY)) ?
				((d->m_channel[ch].m_AUDC & VOLUME_MASK) << (ch * 4)) : 0);
		}

		//if (d->m_out_raw != sum)
		//	d->m_stream->update();

		d->m_old_raw_inval = 0;
		d->m_out_raw = sum;
	}
}

//-------------------------------------------------
//  sound_stream_update - handle update requests for
//  our sound stream
//-------------------------------------------------

void pokey_update(void *info, UINT32 samples, DEV_SMPL **outputs)
{
	pokey_device *d = (pokey_device *)info;
	UINT32 sampindex;

	for(sampindex = 0; sampindex < samples; sampindex++)
	{
		pokey_execute_run(d);

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
			outputs[0][sampindex] = (DEV_SMPL)(d->m_out_filter * 0x7fff);
			outputs[1][sampindex] = (DEV_SMPL)(d->m_out_filter * 0x7fff);
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
			outputs[0][sampindex] = (DEV_SMPL)(V0 * 0x7fff);
			outputs[1][sampindex] = (DEV_SMPL)(V0 * 0x7fff);
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
			outputs[0][sampindex] = (DEV_SMPL)(d->m_out_filter * 0x7fff);
			outputs[1][sampindex] = (DEV_SMPL)(d->m_out_filter * 0x7fff);
		}
		else if (d->m_output_type == DISCRETE_VAR_R)
		{
			/* store sum of output signals into the buffer */

			/* TODO verify that d->m_voltab is in the range -1.0 - +1.0 */
			outputs[0][sampindex] = (DEV_SMPL)(d->m_voltab[d->m_out_raw] * 0x7fff);
			outputs[1][sampindex] = (DEV_SMPL)(d->m_voltab[d->m_out_raw] * 0x7fff);
		}
	}
}

//-------------------------------------------------
//  read - memory interface for reading the active status
//-------------------------------------------------

UINT8 pokey_read(pokey_device *d, UINT8 offset)
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
			//LOG("%s: POKEY read POT%d (final value)  $%02x\n", machine().describe_context(), pot, data);
		}
		else
		{
			data = d->m_pot_counter;
			//LOG("%s: POKEY read POT%d (interpolated) $%02x\n", machine().describe_context(), pot, data);
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
			//LOG("%s: POKEY ALLPOT internal $%02x (reset)\n", machine().describe_context(), data);
		}
		//else if (!d->m_allpot_r_cb.isnull())
		//{
		//	d->m_ALLPOT = data = d->m_allpot_r_cb(offset);
		//	LOG("%s: POKEY ALLPOT callback $%02x\n", machine().describe_context(), data);
		//}
		else
		{
			data = d->m_ALLPOT ^ 0xff;
			//LOG("%s: POKEY ALLPOT internal $%02x\n", machine().describe_context(), data);
		}
		break;

	case KBCODE_C:
		data = d->m_KBCODE;
		break;

	case RANDOM_C:
		if (d->m_AUDCTL & POLY9)
		{
			data = d->m_poly9[d->m_p9] & 0xff;
			//LOG_RAND("%s: POKEY rand9[$%05x]: $%02x\n", machine().describe_context(), m_p9, data);
		}
		else
		{
			data = (d->m_poly17[d->m_p17] >> 8) & 0xff;
			//LOG_RAND("%s: POKEY rand17[$%05x]: $%02x\n", machine().describe_context(), m_p17, data);
		}
		break;

	case SERIN_C:
		//if (!d->m_serin_r_cb.isnull())
		//	d->m_SERIN = d->m_serin_r_cb(offset);
		data = d->m_SERIN;
		//LOG("%s: POKEY SERIN  $%02x\n", machine().describe_context(), data);
		break;

	case IRQST_C:
		/* IRQST is an active low input port; we keep it active high */
		/* internally to ease the (un-)masking of bits */
		data = d->m_IRQST ^ 0xff;
		//LOG("%s: POKEY IRQST  $%02x\n", machine().describe_context(), data);
		break;

	case SKSTAT_C:
		/* SKSTAT is also an active low input port */
		data = d->m_SKSTAT ^ 0xff;
		//LOG("%s: POKEY SKSTAT $%02x\n", machine().describe_context(), data);
		break;

	default:
		//LOG("%s: POKEY register $%02x\n", machine().describe_context(), offset);
		data = 0xff;
		break;
	}
	return data;
}


//-------------------------------------------------
//  write - memory interface for write
//-------------------------------------------------

void pokey_write(pokey_device *d, UINT8 offset, UINT8 data)
{
	/* determine which address was changed */
	switch (offset & 15)
	{
	case AUDF1_C:
		//LOG_SOUND("%s: AUDF1 = $%02x\n", machine().describe_context(), data);
		d->m_channel[CHAN1].m_AUDF = data;
		break;

	case AUDC1_C:
		//LOG_SOUND("%s: POKEY AUDC1  $%02x (%s)\n", machine().describe_context(), data, audc2str(data));
		d->m_channel[CHAN1].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDF2_C:
		//LOG_SOUND("%s: POKEY AUDF2  $%02x\n", machine().describe_context(), data);
		d->m_channel[CHAN2].m_AUDF = data;
		break;

	case AUDC2_C:
		//LOG_SOUND("%s: POKEY AUDC2  $%02x (%s)\n", machine().describe_context(), data, audc2str(data));
		d->m_channel[CHAN2].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDF3_C:
		//LOG_SOUND("%s: POKEY AUDF3  $%02x\n", machine().describe_context(), data);
		d->m_channel[CHAN3].m_AUDF = data;
		break;

	case AUDC3_C:
		//LOG_SOUND("%s: POKEY AUDC3  $%02x (%s)\n", machine().describe_context(), data, audc2str(data));
		d->m_channel[CHAN3].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDF4_C:
		//LOG_SOUND("%s: POKEY AUDF4  $%02x\n", machine().describe_context(), data);
		d->m_channel[CHAN4].m_AUDF = data;
		break;

	case AUDC4_C:
		//LOG_SOUND("%s: POKEY AUDC4  $%02x (%s)\n", machine().describe_context(), data, audc2str(data));
		d->m_channel[CHAN4].m_AUDC = data;
		d->m_old_raw_inval = 1;
		break;

	case AUDCTL_C:
		if( data == d->m_AUDCTL )
			return;
		//LOG_SOUND("%s: POKEY AUDCTL $%02x (%s)\n", machine().describe_context(), data, audctl2str(data));
		d->m_AUDCTL = data;
		d->m_old_raw_inval = 1;
		break;

	case STIMER_C:
	{
		//LOG_TIMER("%s: POKEY STIMER $%02x\n", machine().describe_context(), data);

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
		//LOG("%s: POKEY SKREST $%02x\n", machine().describe_context(), data);
		d->m_SKSTAT &= ~(SK_FRAME|SK_OVERRUN|SK_KBERR);
		break;

	case POTGO_C:
		//LOG("%s: POKEY POTGO  $%02x\n", machine().describe_context(), data);
		if (d->m_SKCTL & SK_RESET)
			pokey_potgo(d);
		break;

	case SEROUT_C:
		//LOG("%s: POKEY SEROUT $%02x\n", machine().describe_context(), data);
		// TODO: convert to real serial comms, fix timings
		// SEROC (1) serial out in progress (0) serial out complete
		// in progress status is necessary for a800 telelnk2 to boot
		d->m_IRQST &= ~IRQ_SEROC;

		//d->m_serout_w_cb(offset, data);
		d->m_SKSTAT |= SK_SEROUT;
		/*
		 * These are arbitrary values, tested with some custom boot
		 * loaders from Ballblazer and Escape from Fractalus
		 * The real times are unknown
		 */
		//d->m_serout_ready_timer->adjust(attotime::from_usec(200));
		/* 10 bits (assumption 1 start, 8 data and 1 stop bit) take how long? */
		//d->m_serout_complete_timer->adjust(attotime::from_usec(2000));
		break;

	case IRQEN_C:
		//LOG("%s: POKEY IRQEN  $%02x\n", machine().describe_context(), data);

		/* acknowledge one or more IRQST bits ? */
		if( d->m_IRQST & ~data )
		{
			/* reset IRQST bits that are masked now, except the SEROC bit (acid5200 pokey_seroc test) */
			d->m_IRQST &= (IRQ_SEROC | data);
		}
		/* store irq enable */
		d->m_IRQEN = data;
		/* if SEROC irq is enabled trigger an irq (acid5200 pokey_seroc test) */
		if (d->m_IRQEN & d->m_IRQST & IRQ_SEROC)
		{
			//LOG_IRQ("POKEY SEROC IRQ enabled\n");
			//m_irq_w_cb(ASSERT_LINE);
		}
		else if (!(d->m_IRQEN & d->m_IRQST))
		{
			//LOG_IRQ("POKEY IRQs all cleared\n");
			//m_irq_w_cb(CLEAR_LINE);
		}
		break;

	case SKCTL_C:
		if( data == d->m_SKCTL )
			return;
		//LOG("%s: POKEY SKCTL  $%02x\n", machine().describe_context(), data);
		d->m_SKCTL = data;
		if( !(data & SK_RESET) )
		{
			pokey_write(d, IRQEN_C,  0);
			pokey_write(d, SKREST_C, 0);
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
		if (!(data & SK_KEYSCAN))
		{
			d->m_SKSTAT &= ~SK_KEYBD;
			d->m_kbd_cnt = 0;
			d->m_kbd_state = 0;
		}
		d->m_old_raw_inval = 1;
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

static void pokey_sid_w(pokey_device *d, UINT8 state)
{
	if (state)
	{
		d->m_SKSTAT |= SK_SERIN;
	}
	else
	{
		d->m_SKSTAT &= ~SK_SERIN;
	}
}


//-------------------------------------------------
//  private stuff
//-------------------------------------------------

INLINE void pokey_process_channel(pokey_device *d, int ch)
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


static void pokey_potgo(pokey_device *d)
{
	int pot;

	//LOG("pokey_potgo\n");

	d->m_ALLPOT = 0x00;
	d->m_pot_counter = 0;

	for( pot = 0; pot < 8; pot++ )
	{
		d->m_POTx[pot] = 228;
#if 0
		if (!m_pot_r_cb[pot].isnull())
		{
			int r = d->m_pot_r_cb[pot](pot);

			LOG("POKEY pot_r(%d) returned $%02x\n", pot, r);
			if (r >= 228)
				r = 228;

			if (r == 0)
			{
				/* immediately set the ready - bit of m_ALLPOT
				 * In this case, most likely no capacitor is connected
				 */
				d->m_ALLPOT |= (1<<pot);
			}

			/* final value */
			d->m_POTx[pot] = r;
		}
#endif
	}
}

static void pokey_vol_init(pokey_device *d)
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
	//if (VERBOSE & LOG_GENERAL)
	if (0)
		for (j=0; j<16; j++)
		{
			rTot = 1.0 / r_chan[j] + 3.0 / r_chan[0];
			rTot = 1.0 / rTot;
			//LOG("%3d - %4.3f\n", j, rTot / (rTot+pull_up)*4.75);
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

static void poly_init_4_5(UINT32 *poly, int size)
{
	//LOG_POLY("poly %d\n", size);

	int mask = (1 << size) - 1;
	UINT32 lfsr = 0;

	int const xorbit = size - 1;
	int i;

	for (i = 0; i < mask; i++)
	{
		lfsr = (lfsr << 1) | (~((lfsr >> 2) ^ (lfsr >> xorbit)) & 1);
		*poly = lfsr & mask;
		poly++;
	}
}

static void poly_init_9_17(UINT32 *poly, int size)
{
	//LOG_RAND("rand %d\n", size);

	int mask = (1 << size) - 1;
	UINT32 lfsr = mask;
	int i;

	if (size == 17)
	{
		for (i = 0; i < mask; i++)
		{
			// calculate next bit @ 7
			const UINT32 in8 = ((lfsr >> 8) & 1) ^ ((lfsr >> 13) & 1);
			const UINT32 in = (lfsr & 1);
			lfsr = lfsr >> 1;
			lfsr = (lfsr & 0xff7f) | (in8 << 7);
			lfsr = (in << 16) | lfsr;
			*poly = lfsr;
			//LOG_RAND("%05x: %02x\n", i, *poly);
			poly++;
		}
	}
	else // size == 9
	{
		for (i = 0; i < mask; i++)
		{
			// calculate next bit
			const UINT32 in = ((lfsr >> 0) & 1) ^ ((lfsr >> 5) & 1);
			lfsr = lfsr >> 1;
			lfsr = (in << 8) | lfsr;
			*poly = lfsr;
			//LOG_RAND("%05x: %02x\n", i, *poly);
			poly++;
		}
	}

}
