/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: player.c,v 1.8 2007-09-15 21:59:56 cmatsuoka Exp $
 */

/*
 * Sat, 18 Apr 1998 20:23:07 +0200  Frederic Bujon <lvdl@bigfoot.com>
 * Pan effect bug fixed: In Fastracker II the track panning effect erases
 * the instrument panning effect, and the same should happen in xmp.
 */

/*
 * Fri, 26 Jun 1998 13:29:25 -0400 (EDT)
 * Reported by Jared Spiegel <spieg@phair.csh.rit.edu>
 * when the volume envelope is not enabled (disabled) on a sample, and a
 * notoff is delivered to ft2 (via either a noteoff in the note column or
 * command Kxx [where xx is # of ticks into row to give a noteoff to the
 * sample]), ft2 will set the volume of playback of the sample to 00h.
 *
 * Claudio's fix: implementing effect K
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "driver.h"
#include "period.h"
#include "effects.h"
#include "player.h"
#include "synth.h"

static int tempo;
static int gvol_slide;
static int gvol_flag;
static int gvol_base;
static double tick_time;
static struct flow_control flow;
static struct xmp_channel* xc_data;
static int* fetch_ctl;

int xmp_scan_ord, xmp_scan_row, xmp_scan_num, xmp_bpm;

/* Vibrato/tremolo waveform tables */
static int waveform[4][64] = {
   {   0,  24,  49,  74,  97, 120, 141, 161, 180, 197, 212, 224,
     235, 244, 250, 253, 255, 253, 250, 244, 235, 224, 212, 197,
     180, 161, 141, 120,  97,  74,  49,  24,   0, -24, -49, -74,
     -97,-120,-141,-161,-180,-197,-212,-224,-235,-244,-250,-253,
    -255,-253,-250,-244,-235,-224,-212,-197,-180,-161,-141,-120,
     -97, -74, -49, -24  },	/* Sine */

   {   0,  -8, -16, -24, -32, -40, -48, -56, -64, -72, -80, -88,
     -96,-104,-112,-120,-128,-136,-144,-152,-160,-168,-176,-184,
    -192,-200,-208,-216,-224,-232,-240,-248, 255, 248, 240, 232,
     224, 216, 208, 200, 192, 184, 176, 168, 160, 152, 144, 136,
     128, 120, 112, 104,  96,  88,  80,  72,  64,  56,  48,  40,
      32,  24,  16,   8  },	/* Ramp down */

   { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
     255, 255, 255, 255, 255, 255, 255, 255,-255,-255,-255,-255,
    -255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,
    -255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,
    -255,-255,-255,-255  },	/* Square */

   {   0,   8,  16,  24,  32,  40,  48,  56,  64,  72,  80,  88,
      96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184,
     192, 200, 208, 216, 224, 232, 240, 248,-255,-248,-240,-232,
    -224,-216,-208,-200,-192,-184,-176,-168,-160,-152,-144,-136,
    -128,-120,-112,-104, -96, -88, -80, -72, -64, -56, -48, -40,
     -32, -24, -16,  -8  }	/* Ramp up */
};

/*
 * "Anyway I think this is the most brilliant piece of crap we
 *  have managed to put up!"
 *                          -- Ice of FC about "Mental Surgery"
 */

struct xxm_header *xxh;
struct xxm_pattern **xxp;		/* Patterns */
struct xxm_track **xxt;			/* Tracks */
struct xxm_instrument_header *xxih;	/* Instrument headers */
struct xxm_instrument_map *xxim;	/* Instrument map */
struct xxm_instrument **xxi;		/* Instruments */
struct xxm_sample *xxs;			/* Samples */
uint16 **xxae;				/* Amplitude envelope */
uint16 **xxpe;				/* Pan envelope */
uint16 **xxfe;				/* Pitch envelope */
struct xxm_channel xxc[64];		/* Channel info */
struct xmp_ord_info xxo_info[XMP_DEF_MAXORD];
int xxo_fstrow[XMP_DEF_MAXORD];
uint8 xxo[XMP_DEF_MAXORD];		/* Orders */

uint8 **med_vol_table;			/* MED volume sequence table */
uint8 **med_wav_table;			/* MED waveform sequence table */

static int module_fetch (struct xxm_event *, int, int);
static void module_play (int, int);

#include "effects.c"


static void dummy ()
{
    /* dummy */
}


static int get_envelope (int16 *env, int p, int x)
{
    int x1, x2, y1, y2;

    if (--p < 0)
	return 64;

    p <<= 1;

    if ((x1 = env[p]) <= x)
	return env[p + 1];

    do {
	p -= 2;
	x1 = env[p];
    } while ((x1 > x) && p);

    y1 = env[p + 1];
    x2 = env[p + 2];
    y2 = env[p + 3];

    return ((y2 - y1) * (x - x1) / (x2 - x1)) + y1;
}


static int
do_envelope (struct xxm_envinfo *ei, uint16 *env, uint16 *x, int rl, int chn)
{
    int loop;

    if (*x < 0xffff)
	(*x)++;

    if (!(ei->flg & XXM_ENV_ON))
	return 0;

    if (ei->npt <= 0)
	return 0;

    if (ei->lps >= ei->npt || ei->lpe >= ei->npt)
	loop = 0;
    else
	loop = ei->flg & XXM_ENV_LOOP;

    if (xmp_ctl->fetch & XMP_CTL_ITENV) {
	if (!rl && (ei->flg & XXM_ENV_SUS)) {
	    if (*x >= env[ei->sue << 1])
		*x = env[ei->sus << 1];
	}
	else if (loop) {
	    if (*x >= env[ei->lpe << 1])
		*x = env[ei->lps << 1];
	}
    }
    else {
	if (!rl && (ei->flg & XXM_ENV_SUS) && *x > env[ei->sus << 1])
	    *x = env[ei->sus << 1];
	if (loop && *x >= env[ei->lpe << 1])
	    if (!(rl && (ei->flg & XXM_ENV_SUS) && ei->sus == ei->lpe))
		*x = env[ei->lps << 1];
    }

    if (*x > env[rl = (ei->npt - 1) << 1]) {
	if (!env[rl + 1])
	    xmp_drv_resetchannel (chn);
	else
	    return xmp_ctl->fetch & XMP_CTL_ENVFADE;
    }
    return 0;
}


static inline int chn_copy (int to, int from)
{
    if (to > 0 && to != from)
	memcpy (&xc_data[to], &xc_data[from], sizeof (struct xmp_channel));

    return to;
}


static inline void chn_reset ()
{
    int i;
    struct xmp_channel *xc;

    synth_reset ();
    memset (xc_data, 0, sizeof (struct xmp_channel) * xmp_ctl->numchn);

    for (i = xmp_ctl->numchn; i--; ) {
	xc = &xc_data[i];
	xc->insdef = xc->ins = xc->key = -1;
    }
    for (i = xmp_ctl->numtrk; i--; ) {
	xc = &xc_data[i];
	xc->masterpan = xxc[i].pan;
	xc->mastervol = 0x40;
        xc->cutoff = 0xff;
    }
}


static inline void chn_fetch (int ord, int row)
{
    int count, chn;

    count = 0;
    for (chn = 0; chn < xxh->chn; chn++)
	if (module_fetch (&EVENT (ord, chn, row), chn, 1) != XMP_OK) {
	    fetch_ctl[chn]++;
	    count++;
	}

    for (chn = 0; count; chn++)
	if (fetch_ctl[chn]) {
	    module_fetch (&EVENT (ord, chn, row), chn, 0);
	    fetch_ctl[chn] = 0;
	    count--;
	}
}


static inline void chn_refresh (int tick)
{ 
    int num;

    for (num = xmp_ctl->numchn; num--;)
	module_play (num, tick);
}


static int module_fetch (struct xxm_event *e, int chn, int ctl)
{
    int xins, ins, smp, note, key, flg;
    struct xmp_channel *xc;

    flg = 0;
    smp = ins = note = -1;
    xc = &xc_data[chn];
    xins = xc->ins;
    key = e->note;

    if (e->ins) {
	ins += e->ins;
	flg = NEW_INS | RESET_VOL | RESET_ENV;

	if (xmp_ctl->fetch & XMP_CTL_OINSMOD) {
	    if (TEST (IS_READY)) {
		xins = xc->insdef;
		RESET (IS_READY);
	    }
	} else if ((uint32)ins < xxh->ins && xxih[ins].nsm) {
	    if (!key && xmp_ctl->fetch & XMP_CTL_INSPRI) {
		if (xins == ins)
		    flg = NEW_INS | RESET_VOL;
		else 
		    key = xc->key + 1;
	    }
	    xins = ins;
	} else {
	    if (!(xmp_ctl->fetch & XMP_CTL_NCWINS))
		xmp_drv_resetchannel (chn);

	    if (xmp_ctl->fetch & XMP_CTL_IGNWINS) {
		ins = -1;
		flg = 0;
	    }
	}

	xc->insdef = ins;
    }

    if (key) {

        if (key == XMP_KEY_FADE) {
            SET (FADEOUT);
	    flg &= ~(RESET_VOL | RESET_ENV);
        } else if (key == XMP_KEY_CUT) {
            xmp_drv_resetchannel (chn);
        } else if (key == XMP_KEY_OFF) {
            SET (RELEASE | KEYOFF);
	    flg &= ~(RESET_VOL | RESET_ENV);
	} else if (e->fxt == FX_TONEPORTA || e->f2t == FX_TONEPORTA) {
	    /* This test should fix portamento behaviour in 7spirits.s3m */
	    if (xmp_ctl->fetch & XMP_CTL_RTGINS && e->ins && xc->ins != ins) {
		flg |= NEW_INS;
		xins = ins;
	    } else if (TEST (KEYOFF)) {
		/* When a toneporta is issued after a keyoff event,
		 * retrigger the instrument (xr-nc.xm, bug #586377)
		 */
		RESET (KEYOFF);
		flg |= NEW_INS;
		xins = ins;
	    } else {
        	key = 0;
	    }
        } else if (flg & NEW_INS) {
            xins = ins;
	    RESET (KEYOFF);
        } else {
            ins = xc->insdef;
            flg |= IS_READY;
	    RESET (KEYOFF);
        }
    }
    if (!key || key >= XMP_KEY_OFF)
	ins = xins;

    if ((uint32)ins < xxh->ins && xxih[ins].nsm)
	flg |= IS_VALID;

    if ((uint32)key < 0x61 && key--) {
	xc->key = key;

        if (flg & IS_VALID) {
	    if (xxim[ins].ins[key] != 0xff) {
		note = key + xxi[ins][xxim[ins].ins[key]].xpo +
		    xxim[ins].xpo[key];
		smp = xxi[ins][xxim[ins].ins[key]].sid;
	    } else
		flg &= ~(RESET_VOL | RESET_ENV | NEW_INS);
	} else
	    if (!(xmp_ctl->fetch & XMP_CTL_CUTNWI))
		xmp_drv_resetchannel (chn);
    }

    if (smp >= 0) {
	if (chn_copy(xmp_drv_setpatch(chn, ins, smp, note,
	    xxi[ins][xxim[ins].ins[key]].nna,
	    xxi[ins][xxim[ins].ins[key]].dct,
	    xxi[ins][xxim[ins].ins[key]].dca, ctl), chn) < 0)
	    return XMP_ERR_VIRTC;
	xc->smp = smp;
    }

    xc->delay = xc->retrig = 0;         /**** Reset flags ****/
    xc->a_idx = xc->a_val[1] = xc->a_val[2] = 0;
    xc->flags = flg | (xc->flags & 0xff000000);

    if ((uint32)xins >= xxh->ins || !xxih[xins].nsm)
	RESET (IS_VALID);
    else
	SET (IS_VALID);
    xc->ins = xins;

    /* Process new volume */
    if (e->vol) {
	xc->volume = e->vol - 1;
	RESET (RESET_VOL);
	SET (NEW_VOL);
    }

    if (TEST (NEW_INS) || (xmp_ctl->fetch & XMP_CTL_OFSRST))
	xc->offset = 0;

    /* Secondary effect is processed _first_ and can be overriden
     * by the primary effect.
     */
    process_fx(chn, e->note, e->f2t, e->f2p, xc);
    process_fx(chn, e->note, e->fxt, e->fxp, xc);

    if (!TEST (IS_VALID)) {
	xc->volume = 0;
	return XMP_OK;
    }

    if (note >= 0) {
	xc->note = note;

	xmp_drv_voicepos (chn, xc->offset);
	if (TEST (OFFSET) && (xmp_ctl->fetch & XMP_CTL_FX9BUG))
	    xc->offset <<= 1;
	RESET (OFFSET);

	/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
	if (!TEST (NEW_PAN))
	    xc->pan = xxi[ins][xxim[ins].ins[key]].pan;

	if (!TEST (FINETUNE))
	    xc->finetune = xxi[ins][xxim[ins].ins[key]].fin;

	xc->s_end = xc->period = note_to_period (note, xxh->flg & XXM_FLG_LINEAR);


	xc->y_idx = xc->t_idx = 0;              /* #?# Onde eu coloco isso? */

	SET (ECHOBACK);
    }

    if (xc->key == 0xff || XXIM.ins[xc->key] == 0xff)
        return XMP_OK;

    if (TEST (RESET_ENV)) {
        xc->fadeout = 0x10000;
        RESET (RELEASE | FADEOUT);
        xc->gvl = XXI[XXIM.ins[xc->key]].gvl;   /* #?# Onde eu coloco isso? */
        xc->insvib_swp = XXI->vsw;              /* #?# Onde eu coloco isso? */
        xc->insvib_idx = 0;                     /* #?# Onde eu coloco isso? */
        xc->v_idx = xc->p_idx = xc->f_idx = 0;
	xc->cutoff = XXI->ifc & 0x80 ? (XXI->ifc - 0x80) * 2 : 0xff;
	xc->resonance = XXI->ifr & 0x80 ? (XXI->ifr - 0x80) * 2 : 0;
    }

    if (TEST (RESET_VOL)) {
        xc->volume = XXI[XXIM.ins[xc->key]].vol;
        SET (ECHOBACK | NEW_VOL);
    }

    if ((xmp_ctl->fetch & XMP_CTL_ST3GVOL) && TEST (NEW_VOL))
	xc->volume = xc->volume * xmp_ctl->volume / xmp_ctl->volbase;

    return XMP_OK;
}


static void module_play (int chn, int t)
{
    struct xmp_channel *xc;
    int finalvol, finalpan, cutoff, act;
    int pan_envelope, frq_envelope;
    uint16 vol_envelope;

    if ((act = xmp_drv_cstat (chn)) == XMP_CHN_DUMB)
	return;

    xc = &xc_data[chn];

    if (!t && act != XMP_CHN_ACTIVE) {
	if (!TEST (IS_VALID) || act == XMP_ACT_CUT) {
	    xmp_drv_resetchannel (chn);
	    return;
	}
	xc->a_val[1] = xc->a_val[2] = 0;
	xc->delay = xc->retrig = xc->a_idx = 0;
	xc->flags &= (0xff000000 | IS_VALID);
    }

    if (!TEST (IS_VALID))
	return;

    /* Process MED synth instruments */
    xmp_med_synth (chn, xc, !t && TEST (NEW_INS));

    if (TEST (RELEASE) && !(XXIH.aei.flg & XXM_ENV_ON))
	xc->fadeout = 0;

    if (TEST (FADEOUT | RELEASE) || act == XMP_ACT_FADE || act == XMP_ACT_OFF)
	if (!(xc->fadeout = xc->fadeout > XXIH.rls ?
	    xc->fadeout - XXIH.rls : 0)) {
	    xmp_drv_resetchannel (chn);
	    return;
	}

    vol_envelope = XXIH.aei.flg & XXM_ENV_ON ?
	get_envelope((int16 *)XXAE, XXIH.aei.npt, xc->v_idx) : 64;

    pan_envelope = XXIH.pei.flg & XXM_ENV_ON ?
	get_envelope((int16 *)XXPE, XXIH.pei.npt, xc->p_idx) : 32;

    frq_envelope = XXIH.fei.flg & XXM_ENV_ON ?
        (int16)get_envelope((int16 *)XXFE, XXIH.fei.npt, xc->f_idx) : 0;

    /* Update envelopes */
    if (do_envelope (&XXIH.aei, XXAE, &xc->v_idx, DOENV_RELEASE, chn))
	SET (FADEOUT);
    do_envelope (&XXIH.pei, XXPE, &xc->p_idx, DOENV_RELEASE, -1323);
    do_envelope (&XXIH.fei, XXFE, &xc->f_idx, DOENV_RELEASE, -1137);

    /* Do cut/retrig */
    if (xc->retrig) {
	if (!--xc->rcount) {
	    xmp_drv_retrig (chn);
	    xc->volume += rval[xc->rtype].s;
	    xc->volume *= rval[xc->rtype].m;
	    xc->volume /= rval[xc->rtype].d;
	    xc->rcount = xc->retrig;
	}
    }

    finalvol = xc->volume;

    if (TEST (TREMOLO))
	finalvol += waveform[xc->t_type][xc->t_idx] * xc->t_depth / 512;
    if (finalvol > gvol_base)
	finalvol = gvol_base;
    if (finalvol < 0)
	finalvol = 0;

    finalvol = (finalvol * xc->fadeout) >> 6;	/* 16 bit output */

    finalvol = (uint32) (vol_envelope *
	(xmp_ctl->fetch & XMP_CTL_ST3GVOL ? 0x40 : xmp_ctl->volume) *
	xc->mastervol / 0x40 * ((int) finalvol * 0x40 / gvol_base)) >> 20;

    /* Volume translation table (for PTM) */
    if (xmp_ctl->vol_xlat)
	finalvol = xmp_ctl->vol_xlat[finalvol >> 2] << 2;

    if (xxh->flg & XXM_FLG_INSVOL)
	finalvol = (finalvol * XXIH.vol * xc->gvl) >> 12;

    /* IT pitch envelopes are always linear, even in Amiga period mode.
     * Each unit in the envelope scale is 1/25 semitone.
     */
    xc->pitchbend = period_to_bend (xc->period + (TEST (VIBRATO) ?
	(waveform[xc->y_type][xc->y_idx] * xc->y_depth) >> 10 : 0) +
	waveform[XXI->vwf][xc->insvib_idx] * XXI->vde / (1024 *
	(1 + xc->insvib_swp)),
	xc->note,
	xc->finetune,
	xxh->flg & XXM_FLG_MODRNG,
	xc->gliss,
	xxh->flg & XXM_FLG_LINEAR) +
	(XXIH.fei.flg & XXM_ENV_FLT ? 0 : frq_envelope);

    /* From Takashi Iwai's awedrv FAQ:
     *
     * Q3.9: Many clicking noises can be heard in some midi files.
     *    A: If this happens when panning status changes, it is due to the
     *       restriction of Emu8000 chip. Try -P option with drvmidi. This
     *       option suppress the realtime pan position change. Otherwise,
     *       it may be a bug.
     */

    finalpan = xmp_ctl->fetch & XMP_CTL_DYNPAN ?
	xc->pan + (pan_envelope - 32) * (128 - abs (xc->pan - 128)) / 32 : 0x80;
    finalpan = xc->masterpan + (finalpan - 128) *
	(128 - abs (xc->masterpan - 128)) / 128;

    cutoff = XXIH.fei.flg & XXM_ENV_FLT ? frq_envelope : 0xff;
    cutoff = xc->cutoff * cutoff / 0xff;

    /* Echoback events */
    if (chn < xmp_ctl->numtrk) {
	xmp_drv_echoback ((finalpan << 12)|(chn << 4)|XMP_ECHO_CHN);

	if (TEST (ECHOBACK | PITCHBEND | TONEPORTA)) {
	    xmp_drv_echoback ((xc->key << 12)|(xc->ins << 4)|XMP_ECHO_INS);
	    xmp_drv_echoback ((xc->volume << 4) * 0x40 / gvol_base |
		XMP_ECHO_VOL);

	    xmp_drv_echoback ((xc->pitchbend << 4)|XMP_ECHO_PBD);
	}
    }

    /* Do delay */
    if (xc->delay) {
	if (--xc->delay)
	    finalvol = 0;
	else
	    xmp_drv_retrig (chn);
    }

    /* Do tremor */
    if (xc->tremor) {
	if (xc->tcnt_dn > 0)
	    finalvol = 0;
	if (!xc->tcnt_up--)
	    xc->tcnt_dn = LSN (xc->tremor);
	if (!xc->tcnt_dn--)
	    xc->tcnt_up = MSN (xc->tremor);
    }

    /* Do keyoff */
    if (xc->keyoff) {
	if (!--xc->keyoff)
	    SET (RELEASE);
    }

    /* Volume slides happen in all frames but the first, except when the
     * "volume slide on all frames" flag is set.
     */
    if (t % tempo || xmp_ctl->fetch & XMP_CTL_VSALL) {
	if (!chn && gvol_flag) {
	    xmp_ctl->volume += gvol_slide;
	    if (xmp_ctl->volume < 0)
		xmp_ctl->volume = 0;
	    else if (xmp_ctl->volume > gvol_base)
		xmp_ctl->volume = gvol_base;
	}
	if (TEST (VOL_SLIDE))
	    xc->volume += xc->v_val;

	if (TEST (VOL_SLIDE_2))
	    xc->volume += xc->v_val2;

	if (TEST (TRK_VSLIDE))
	    xc->mastervol += xc->trk_val;
    }

    /* "Fine" sliding effects are processed in the first frame of each row,
     * and standard slides in the rest of the frames.
     */
    if (t % tempo || xmp_ctl->fetch & XMP_CTL_PBALL) {
	/* Do pan and pitch sliding */
	if (TEST (PAN_SLIDE)) {
	    xc->pan += xc->p_val;
	    if (xc->pan < 0)
		xc->pan = 0;
	    else if (xc->pan > 0xff)
		xc->pan = 0xff;
	}

	if (TEST (PITCHBEND))
	    xc->period += xc->f_val;

	/* Workaround for panic.s3m (from Toru Egashira's NSPmod) */
	if (xc->period <= 8)
	    xc->volume = 0;
    }

    if (t % tempo == 0) {
	/* Process "fine" effects */
	if (TEST (FINE_VOLS))
            xc->volume += xc->v_fval;
	if (TEST (FINE_BEND))
	    xc->period = (4 * xc->period + xc->f_fval) / 4;
	if (TEST (TRK_FVSLIDE))
            xc->mastervol += xc->trk_fval;
    }

    if (xc->volume < 0)
	xc->volume = 0;
    else if (xc->volume > gvol_base)
	xc->volume = gvol_base;

    if (xc->mastervol < 0)
	xc->mastervol = 0;
    else if (xc->mastervol > gvol_base)
	xc->mastervol = gvol_base;

    if (xxh->flg & XXM_FLG_LINEAR) {
	if (xc->period < MIN_PERIOD_L)
	    xc->period = MIN_PERIOD_L;
	else if (xc->period > MAX_PERIOD_L)
	    xc->period = MAX_PERIOD_L;
    } else {
	if (xc->period < MIN_PERIOD_A)
	    xc->period = MIN_PERIOD_A;
	else if (xc->period > MAX_PERIOD_A)
	    xc->period = MAX_PERIOD_A;
    }

    /* Do tone portamento */
    if (TEST (TONEPORTA)) {
	xc->period += (xc->s_sgn * xc->s_val);
	if ((xc->s_sgn * xc->s_end) <= (xc->s_sgn * xc->period)) {
	    xc->period = xc->s_end;
	    RESET (TONEPORTA);
	}
    }

    /* Update vibrato, tremolo and arpeggio indexes */
    xc->insvib_idx += XXI->vra >> 2;
    xc->insvib_idx %= 64;
    if (xc->insvib_swp > 1)
	xc->insvib_swp -= 2;
    else
	xc->insvib_swp = 0;
    xc->y_idx += xc->y_rate;
    xc->y_idx %= 64;
    xc->t_idx += xc->t_rate;
    xc->t_idx %= 64;
    xc->a_idx++;
    xc->a_idx %= 3;

    /* Adjust pitch and pan, than play the note */
    finalpan = xmp_ctl->outfmt & XMP_FMT_MONO ?
	0 : (finalpan - 0x80) * xmp_ctl->mix / 100;
    xmp_drv_setbend (chn, (xc->pitchbend + xc->a_val[xc->a_idx]));
    xmp_drv_setpan (chn, xmp_ctl->fetch & XMP_CTL_REVERSE ?
	-finalpan : finalpan);
    xmp_drv_setvol (chn, finalvol >> 2);

    if (cutoff < 0xff && (xmp_ctl->fetch & XMP_CTL_FILTER)) {
	filter_setup (xc, cutoff);
	xmp_drv_seteffect (chn, XMP_FX_FILTER_B0, xc->flt_B0);
	xmp_drv_seteffect (chn, XMP_FX_FILTER_B1, xc->flt_B1);
	xmp_drv_seteffect (chn, XMP_FX_FILTER_B2, xc->flt_B2);
    } else {
	cutoff = 0xff;
    }

    xmp_drv_seteffect (chn, XMP_FX_RESONANCE, xc->resonance);
    xmp_drv_seteffect (chn, XMP_FX_CUTOFF, cutoff);
    xmp_drv_seteffect (chn, XMP_FX_CHORUS, xxc[chn].cho);
    xmp_drv_seteffect (chn, XMP_FX_REVERB, xxc[chn].rvb);
}


int xmpi_player_start()
{
    int r, o, t, e;		/* rows, order, tick, end point */
    double playing_time;

    if (!xmp_ctl)
	return XMP_ERR_DINIT;

    if (xmp_event_callback == NULL)
	xmp_event_callback = dummy;

    gvol_slide = 0;
    gvol_base = xmp_ctl->volbase;
    xmp_ctl->volume = xxo_info[o = xmp_ctl->start].gvl;
    tick_time = xmp_ctl->rrate / (xmp_bpm = xxo_info[o].bpm);
    flow.jumpline = xxo_fstrow[o];
    tempo = xxo_info[o].tempo;
    playing_time = 0;

    if ((r = xmp_drv_on (xxh->chn)) != XMP_OK)
	return r;

    flow.jump = -1;

    fetch_ctl = calloc (xxh->chn, sizeof (int));
    flow.loop_stack = calloc (xmp_ctl->numchn, sizeof (int));
    flow.loop_row = calloc (xmp_ctl->numchn, sizeof (int));
    xc_data = calloc (xmp_ctl->numchn, sizeof (struct xmp_channel));
    if (!(fetch_ctl && flow.loop_stack && flow.loop_row && xc_data))
	return XMP_ERR_ALLOC;

    chn_reset ();

    xmp_drv_starttimer ();

    for (e = xmp_scan_num; ; o++) {
next_order:
	if (o >= xxh->len) {
	    o = ((uint32)xxh->rst > xxh->len ||
		(uint32)xxo[xxh->rst] >= xxh->pat) ? 0 : xxh->rst;
	    xmp_ctl->volume = xxo_info[o].gvl;
	    if (xxo[o] == 0xff)
		break;
	}
	if (xxo[o] >= xxh->pat) {
	    if (xxo[o] == 0xff)		/* S3M uses 0xff as end mark */
		o = xxh->len;
	    continue;
	}

	r = xxp[xxo[o]]->rows;
	if (flow.jumpline >= r)
	    flow.jumpline = 0;
	flow.row_cnt = flow.jumpline;
	flow.jumpline = 0;

	xmp_ctl->pos = o;

	for (; flow.row_cnt < r; flow.row_cnt++) {
	    if ((~xmp_ctl->fetch & XMP_CTL_LOOP) && o == xmp_scan_ord &&
		flow.row_cnt == xmp_scan_row) {
		if (!e--)
		    goto end_module;
	    }

	    if (xmp_ctl->pause) {
		xmp_drv_stoptimer ();
		while (xmp_ctl->pause) {
		    xmpi_select_read (1, 125);
		    xmp_event_callback (0);
		}
		xmp_drv_starttimer ();
	    }

	    for (t = 0; t < (tempo * (1 + flow.delay)); t++) {

		/* xmp_player_ctl processing */

		if (o != xmp_ctl->pos) {
		    if (xmp_ctl->pos == -1)
			xmp_ctl->pos++;		/* restart module */

		    if (xmp_ctl->pos == -2) {	/* set by xmp_module_stop */
			xmp_drv_bufwipe ();
			goto end_module;	/* that's all folks */
		    }

		    if (xmp_ctl->pos == 0)
			e = xmp_scan_num;

		    tempo = xxo_info[o = xmp_ctl->pos].tempo;
		    tick_time = xmp_ctl->rrate / (xmp_bpm = xxo_info[o].bpm);
		    xmp_ctl->volume = xxo_info[o].gvl;
		    flow.jump = o;
		    flow.jumpline = xxo_fstrow[o--];
		    flow.row_cnt = -1;
		    xmp_drv_bufwipe ();
		    xmp_drv_sync (0);
		    xmp_drv_reset ();
		    chn_reset ();
		    break;
		}

		if (!t) {
		    gvol_flag = 0;
		    chn_fetch(xxo[o], flow.row_cnt);

		    xmp_drv_echoback((tempo << 12)|(xmp_bpm << 4)|
			XMP_ECHO_BPM);
		    xmp_drv_echoback((xmp_ctl->volume << 4) | XMP_ECHO_GVL);
		    xmp_drv_echoback((xxo[o] << 12)|(o << 4) | XMP_ECHO_ORD);
		    xmp_drv_echoback((xmp_ctl->numvoc << 4) | XMP_ECHO_NCH);
		    xmp_drv_echoback(((xxp[xxo[o]]->rows - 1) << 12) |
					(flow.row_cnt << 4)|XMP_ECHO_ROW);
		}
		xmp_drv_echoback ((t << 4) | XMP_ECHO_FRM);
		chn_refresh (t);

		if (xmp_ctl->time && (xmp_ctl->time < playing_time))
		    goto end_module;

		playing_time += xmp_ctl->rrate / (100 * xmp_bpm);

		if (xmp_ctl->fetch & XMP_CTL_MEDBPM)
		    xmp_drv_sync (tick_time * 33 / 125);
		else
		    xmp_drv_sync (tick_time);

		xmp_drv_bufdump ();
	    }

	    flow.delay = 0;

	    if (flow.row_cnt == -1)
		break;

	    if (flow.pbreak) {
		flow.pbreak = 0;
		break;
	    }

	    if (flow.loop_chn) {
		flow.row_cnt = flow.loop_row[--flow.loop_chn] - 1;
		flow.loop_chn = 0;
	    }
	}

	if (flow.jump != -1) {
	    o = flow.jump;
	    flow.jump = -1;
	    goto next_order;
	}
    }

end_module:
    xmp_drv_echoback (XMP_ECHO_END);
    while (xmp_drv_getmsg () != XMP_ECHO_END)
	xmp_drv_bufdump ();
    xmp_drv_stoptimer ();

    free (xc_data);
    free (flow.loop_row);
    free (flow.loop_stack);
    free (fetch_ctl);

    xmp_drv_off ();

    return XMP_OK;
}
