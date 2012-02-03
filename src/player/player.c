/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "driver.h"
#include "period.h"
#include "effects.h"
#include "player.h"
#include "synth.h"
#include "mixer.h"

/* Values for multi-retrig */
static struct retrig_t rval[] = {
    {   0,  1,  1 }, {  -1,  1,  1 }, {  -2,  1,  1 }, {  -4,  1,  1 },
    {  -8,  1,  1 }, { -16,  1,  1 }, {   0,  2,  3 }, {   0,  1,  2 },
    {   0,  1,  1 }, {   1,  1,  1 }, {   2,  1,  1 }, {   4,  1,  1 },
    {   8,  1,  1 }, {  16,  1,  1 }, {   0,  3,  2 }, {   0,  2,  1 },
    {   0,  0,  1 }	/* Note cut */
};

#define WAVEFORM_SIZE 64

/* Vibrato/tremolo waveform tables */
static int waveform[4][WAVEFORM_SIZE] = {
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

static struct xmp_event empty_event = { 0, 0, 0, 0, 0, 0, 0 };

/*
 * "Anyway I think this is the most brilliant piece of crap we
 *  have managed to put up!"
 *			  -- Ice of FC about "Mental Surgery"
 */

static int read_event (struct xmp_context *, struct xmp_event *, int, int);
static void play_channel (struct xmp_context *, int, int);


/* Instrument vibrato */

static inline int
get_instrument_vibrato(struct xmp_mod_context *m, struct channel_data *xc)
{
	struct xmp_subinstrument *sub = &XXI;
	int type = sub->vwf;
	int depth = sub->vde;
	int phase = xc->instrument_vibrato.phase;
	int sweep = xc->instrument_vibrato.sweep;

	return waveform[type][phase] * depth / (1024 * (1 + sweep));
}

static inline void
update_instrument_vibrato(struct xmp_mod_context *m, struct channel_data *xc)
{
	struct xmp_subinstrument *sub = &XXI;
	int rate = sub->vra;

	xc->instrument_vibrato.phase += rate >> 2;
	xc->instrument_vibrato.phase %= WAVEFORM_SIZE;

	if (xc->instrument_vibrato.sweep > 1)
		xc->instrument_vibrato.sweep -= 2;
	else
		xc->instrument_vibrato.sweep = 0;
}


static inline void copy_channel(struct xmp_player_context *p, int to, int from)
{
	if (to > 0 && to != from) {
		memcpy(&p->xc_data[to], &p->xc_data[from],
					sizeof (struct channel_data));
	}
}


static inline void reset_channel(struct xmp_context *ctx)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &ctx->m;
    struct channel_data *xc;
    int i;

    m->synth->reset(ctx);
    memset(p->xc_data, 0, sizeof (struct channel_data) * d->numchn);

    for (i = d->numchn; i--; ) {
	xc = &p->xc_data[i];
	xc->insdef = xc->ins = xc->key = -1;
    }
    for (i = d->numtrk; i--; ) {
	xc = &p->xc_data[i];
	xc->masterpan = m->mod.xxc[i].pan;
	xc->mastervol = m->mod.xxc[i].vol;
	xc->filter.cutoff = 0xff;
    }
}


static int read_event(struct xmp_context *ctx, struct xmp_event *e, int chn, int ctl)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &ctx->m;
    int xins, ins, smp, note, key, flg;
    struct channel_data *xc;
    int cont_sample;

    xc = &p->xc_data[chn];

    /* Tempo affects delay and must be computed first */
    if ((e->fxt == FX_TEMPO && e->fxp < 0x20) || e->fxt == FX_S3M_TEMPO) {
	if (e->fxp)
	    p->tempo = e->fxp;
    }
    if ((e->f2t == FX_TEMPO && e->f2p < 0x20) || e->f2t == FX_S3M_TEMPO) {
	if (e->f2p)
	    p->tempo = e->f2p;
    }

    /* Delay the entire fetch cycle */
    if (e->fxt == FX_EXTENDED && MSN(e->fxp) == EX_DELAY) {
	xc->delay = LSN(e->fxp) + 1;
	xc->delayed_event = e;
	if (e->ins)
		xc->delayed_ins = e->ins;
	e->fxt = e->fxp = 0;
	return 0;
    }
    if (e->f2t == FX_EXTENDED && MSN(e->f2p) == EX_DELAY) {
	xc->delay = LSN(e->f2p) + 1;
	xc->delayed_event = e;
	if (e->ins)
		xc->delayed_ins = e->ins;
	e->f2t = e->f2p = 0;
	return 0;
    }

    /* Emulate Impulse Tracker "always read instrument" bug */
    if (e->note && !e->ins && xc->delayed_ins && HAS_QUIRK(QUIRK_SAVEINS)) {
	e->ins = xc->delayed_ins;
	xc->delayed_ins = 0;
    }

    flg = 0;
    smp = ins = note = -1;
    xins = xc->ins;
    key = e->note;
    cont_sample = 0;

    /* Check instrument */

    if (e->ins) {
	ins = e->ins - 1;
	flg = NEW_INS | RESET_VOL | RESET_ENV;
	xc->fadeout = 0x8000;	/* for painlace.mod pat 0 ch 3 echo */
	xc->per_flags = 0;

	/* Benjamin Shadwick <benshadwick@gmail.com> informs that Vestiges.xm
	 * has sticky note issues around time 0:51. Tested it with FT2 and
	 * a new note with invalid instrument should cut current note
	 */
	/*if (HAS_QUIRK(QUIRK_OINSMOD)) {
	    if (TEST(IS_READY)) {
		xins = xc->insdef;
		RESET(IS_READY);
	    }
	} else */

	if ((uint32)ins < m->mod.ins && m->mod.xxi[ins].nsm) {	/* valid ins */
	    if (!key && HAS_QUIRK(QUIRK_INSPRI)) {
		if (xins == ins)
		    flg = NEW_INS | RESET_VOL;
		else 
		    key = xc->key + 1;
	    }
	    xins = ins;
	} else {					/* invalid ins */
	    if (!HAS_QUIRK(QUIRK_NCWINS))
		xmp_drv_resetchannel(ctx, chn);

	    if (HAS_QUIRK(QUIRK_IGNWINS)) {
		ins = -1;
		flg = 0;
	    }
	}

	xc->insdef = ins;

	xc->med.arp = xc->med.aidx = 0;
    }

    /* Check note */

    if (key) {
	flg |= NEW_NOTE;

	if (key == XMP_KEY_FADE) {
	    SET(FADEOUT);
	    flg &= ~(RESET_VOL | RESET_ENV);
	} else if (key == XMP_KEY_CUT) {
	    xmp_drv_resetchannel(ctx, chn);
	} else if (key == XMP_KEY_OFF) {
	    SET(RELEASE);
	    flg &= ~(RESET_VOL | RESET_ENV);
	} else if (e->fxt == FX_TONEPORTA || e->f2t == FX_TONEPORTA
		|| e->fxt == FX_TONE_VSLIDE || e->f2t == FX_TONE_VSLIDE) {
	    /* Fix portamento in 7spirits.s3m and mod.Biomechanoid */
	    if (HAS_QUIRK(QUIRK_RTGINS) && e->ins && xc->ins != ins) {
		flg |= NEW_INS;
		xins = ins;
	    } else {
		/* When a toneporta is issued after a keyoff event,
		 * retrigger the instrument (xr-nc.xm, bug #586377)
		 *
		 *   flg |= NEW_INS;
		 *   xins = ins;
		 *
		 * (From Decibelter - Cosmic 'Wegian Mamas.xm p04 ch7)
		 * We don't retrigger the sample, it simply continues.
		 * This is important to play sweeps and loops correctly.
		 */
		cont_sample = 1;

		/* set key to 0 so we can have the tone portamento from
		 * the original note (see funky_stars.xm pos 5 ch 9)
		 */
		key = 0;

		/* And do the same if there's no keyoff (see comic bakery
		 * remix.xm pos 1 ch 3)
		 */
	    }
	} else if (flg & NEW_INS) {
	    xins = ins;
	} else {
	    ins = xc->insdef;
	    flg |= IS_READY;
	}
    }

    if (!key || key >= XMP_KEY_OFF)
	ins = xins;

    if ((uint32)ins < m->mod.ins && m->mod.xxi[ins].nsm)
	flg |= IS_VALID;

    if ((uint32)key < XMP_KEY_OFF && key > 0) {
	xc->key = --key;

	if (flg & IS_VALID && key < XXM_KEY_MAX) {
	    if (m->mod.xxi[ins].map[key].ins != 0xff) {
		int mapped = m->mod.xxi[ins].map[key].ins;
		int transp = m->mod.xxi[ins].map[key].xpo;

		note = key + m->mod.xxi[ins].sub[mapped].xpo + transp;
		smp = m->mod.xxi[ins].sub[mapped].sid;
	    } else {
		flg &= ~(RESET_VOL | RESET_ENV | NEW_INS | NEW_NOTE);
	    }
	} else {
	    if (!HAS_QUIRK(QUIRK_CUTNWI))
		xmp_drv_resetchannel(ctx, chn);
	}
    }

    if (smp >= 0) {
	int mapped = m->mod.xxi[ins].map[key].ins;
	int to = xmp_drv_setpatch(ctx, chn, ins, smp, note,
			m->mod.xxi[ins].sub[mapped].nna, m->mod.xxi[ins].sub[mapped].dct,
			m->mod.xxi[ins].sub[mapped].dca, ctl, cont_sample);

	if (to < 0)
		return -1;

	copy_channel(p, to, chn);

	xc->smp = smp;
    }

    /* Reset flags */
    xc->delay = xc->retrig = 0;
    xc->flags = flg | (xc->flags & 0xff000000);	/* keep persistent flags */

    reset_stepper(&xc->arpeggio);
    xc->tremor = 0;

    if ((uint32)xins >= m->mod.ins || !m->mod.xxi[xins].nsm) {
	RESET(IS_VALID);
    } else {
	SET(IS_VALID);
    }

    xc->ins = xins;

    /* Process new volume */
    if (e->vol) {
	xc->volume = e->vol - 1;
	RESET(RESET_VOL);
	SET(NEW_VOL);
    }

    if (TEST(NEW_INS) || HAS_QUIRK(QUIRK_OFSRST))
	xc->offset_val = 0;

    /* Secondary effect is processed _first_ and can be overriden
     * by the primary effect.
     */
    process_fx(ctx, chn, e->note, e->f2t, e->f2p, xc, 1);
    process_fx(ctx, chn, e->note, e->fxt, e->fxp, xc, 0);

    if (!TEST(IS_VALID)) {
	xc->volume = 0;
	return 0;
    }

    if (note >= 0) {
	int mapped = m->mod.xxi[ins].map[key].ins;

	xc->note = note;

	if (cont_sample == 0) {
	    xmp_drv_voicepos(ctx, chn, xc->offset_val);
	    if (TEST(OFFSET) && HAS_QUIRK(QUIRK_FX9BUG))
		xc->offset_val <<= 1;
	}
	RESET(OFFSET);

	/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
	if (!TEST(NEW_PAN))
	    xc->pan = m->mod.xxi[ins].sub[mapped].pan;

	if (!TEST(FINETUNE))
	    xc->finetune = m->mod.xxi[ins].sub[mapped].fin;

	xc->s_end = xc->period = note_to_period(note, xc->finetune,
			m->mod.flg & XXM_FLG_LINEAR);

	set_lfo_phase(&xc->vibrato, 0);
	set_lfo_phase(&xc->tremolo, 0);
    }

    if (xc->key < 0 || m->mod.xxi[xc->ins].map[xc->key].ins == 0xff)
	return 0;

    if (TEST(RESET_ENV)) {
	/* xc->fadeout = 0x8000; -- moved to fetch */
	RESET(RELEASE | FADEOUT);

	/* H: where should I put these? */
	xc->gvl = XXI.gvl;
	xc->instrument_vibrato.sweep = XXI.vsw;
	xc->instrument_vibrato.phase = 0;

	xc->v_idx = xc->p_idx = xc->f_idx = 0;
	xc->filter.cutoff = XXI.ifc & 0x80 ? (XXI.ifc - 0x80) * 2 : 0xff;
	xc->filter.resonance = XXI.ifr & 0x80 ? (XXI.ifr - 0x80) * 2 : 0;
    }

    if (TEST(RESET_VOL)) {
	xc->volume = XXI.vol;
	SET(NEW_VOL);
    }

    if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL))
	xc->volume = xc->volume * m->volume / m->volbase;

    return 0;
}


static inline void read_row(struct xmp_context *ctx, int pat, int row)
{
    int count, chn;
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &ctx->m;
    struct xmp_event *event;

    count = 0;
    for (chn = 0; chn < m->mod.chn; chn++) {
	if (row < m->mod.xxt[TRACK_NUM(pat, chn)]->rows) {
	    event = &EVENT(pat, chn, row);
	} else {
	    event = &empty_event;
	}

	if (read_event(ctx, event, chn, 1) != 0) {
	    p->fetch_ctl[chn]++;
	    count++;
	}
    }

    for (chn = 0; count; chn++) {
	if (p->fetch_ctl[chn]) {
	    if (row < m->mod.xxt[TRACK_NUM(pat, chn)]->rows) {
	        event = &EVENT(pat, chn, row);
	    } else {
	        event = &empty_event;
	    }
	    read_event(ctx, &EVENT(pat, chn, row), chn, 0);
	    p->fetch_ctl[chn] = 0;
	    count--;
	}
    }
}


static void play_channel(struct xmp_context *ctx, int chn, int t)
{
    struct channel_data *xc;
    int finalvol, finalpan, cutoff, act;
    int pan_envelope, frq_envelope;
    int med_arp, vibrato, med_vibrato;
    uint16 vol_envelope;
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &ctx->m;
    struct xmp_options *o = &ctx->o;

    xc = &p->xc_data[chn];

    /* Do delay */
    if (xc->delay && !--xc->delay) {
	if (read_event(ctx, xc->delayed_event, chn, 1) != 0)
	    read_event(ctx, xc->delayed_event, chn, 0);
    }

    if ((act = xmp_drv_cstat(ctx, chn)) == XMP_CHN_DUMB)
	return;

    if (!t && act != XMP_CHN_ACTIVE) {
	if (!TEST(IS_VALID) || act == XMP_ACT_CUT) {
	    xmp_drv_resetchannel(ctx, chn);
	    return;
	}
	xc->delay = xc->retrig = 0;
	reset_stepper(&xc->arpeggio);
	xc->flags &= (0xff000000 | IS_VALID);	/* keep persistent flags */
    }

    if (!TEST(IS_VALID))
	return;

    /* Process MED synth instruments */
    xmp_med_synth(ctx, chn, xc, !t && TEST(NEW_INS | NEW_NOTE));

    if (TEST(RELEASE) && !(XXIH.aei.flg & XXM_ENV_ON))
	xc->fadeout = 0;
 
    if (TEST(FADEOUT | RELEASE) || act == XMP_ACT_FADE || act == XMP_ACT_OFF) {
	xc->fadeout = xc->fadeout > XXIH.rls ? xc->fadeout - XXIH.rls : 0;

	if (xc->fadeout == 0) {

	    /* Setting the volume to 0 instead of resetting the channel
	     * will make us spend more CPU, but allows portamento after
	     * keyoff to continue the sample instead of resetting it.
	     * This is used in Decibelter - Cosmic 'Wegian Mamas.xm
	     * Only reset the channel in virtual channel mode so we
	     * can release it.
	     */
	     if (m->flags & XMP_CTL_VIRTUAL) {
		 xmp_drv_resetchannel(ctx, chn);
		 return;
	     } else {
		 xc->volume = 0;
	     }
	}
    }

    vol_envelope = get_envelope(&XXIH.aei, xc->v_idx, 64);
    pan_envelope = get_envelope(&XXIH.pei, xc->p_idx, 32);
    frq_envelope = get_envelope(&XXIH.fei, xc->f_idx, 0);

    /* Update envelopes */
    xc->v_idx = update_envelope(&XXIH.aei, xc->v_idx, DOENV_RELEASE);
    xc->p_idx = update_envelope(&XXIH.pei, xc->p_idx, DOENV_RELEASE);
    xc->f_idx = update_envelope(&XXIH.fei, xc->f_idx, DOENV_RELEASE);

    switch (check_envelope_fade(&XXIH.aei, xc->v_idx)) {
    case -1:
	xmp_drv_resetchannel(ctx, chn);
	break;
    case 0:
	break;
    default:
	if (HAS_QUIRK(QUIRK_ENVFADE)) {
		SET(FADEOUT);
	}
    }

    /* Do note slide */
    if (TEST(NOTE_SLIDE)) {
	if (!--xc->ns_count) {
	    xc->note += xc->ns_val;
	    xc->period = note_to_period(xc->note, xc->finetune,
				m->mod.flg & XXM_FLG_LINEAR);
	    xc->ns_count = xc->ns_speed;
	}
    }

    /* Do cut/retrig */
    if (xc->retrig) {
	if (!--xc->rcount) {
	    if (xc->rtype < 0x10)
		xmp_drv_retrig(ctx, chn);	/* don't retrig on cut */
	    xc->volume += rval[xc->rtype].s;
	    xc->volume *= rval[xc->rtype].m;
	    xc->volume /= rval[xc->rtype].d;
	    xc->rcount = xc->retrig;
	}
    }

    finalvol = xc->volume;

    if (TEST(TREMOLO))
	finalvol += get_lfo(&xc->tremolo) / 512;
    if (finalvol > p->gvol_base)
	finalvol = p->gvol_base;
    if (finalvol < 0)
	finalvol = 0;

    finalvol = (finalvol * xc->fadeout) >> 5;	/* 16 bit output */

    finalvol = (uint32) (vol_envelope *
	(HAS_QUIRK(QUIRK_ST3GVOL) ? 0x40 : m->volume) *
	xc->mastervol / 0x40 * ((int)finalvol * 0x40 / p->gvol_base)) >> 18;

    /* Volume translation table (for PTM, ARCH, COCO) */
    if (m->vol_table) {
	finalvol = m->volbase == 0xff ?
		m->vol_table[finalvol >> 2] << 2 :
		m->vol_table[finalvol >> 4] << 4;
    }

    if (m->mod.flg & XXM_FLG_INSVOL)
	finalvol = (finalvol * XXIH.vol * xc->gvl) >> 12;


    /* Vibrato */

    med_vibrato = get_med_vibrato(xc);

    vibrato = get_instrument_vibrato(m, xc);
    if (TEST(VIBRATO) || TEST_PER(VIBRATO)) {
	vibrato += get_lfo(&xc->vibrato) >> 10;
    }


    /* IT pitch envelopes are always linear, even in Amiga period mode.
     * Each unit in the envelope scale is 1/25 semitone.
     */
    xc->pitchbend = period_to_bend(
	xc->period + vibrato + med_vibrato,
	xc->note,
	/* xc->finetune, */
	m->mod.flg & XXM_FLG_MODRNG,
	xc->gliss,
	m->mod.flg & XXM_FLG_LINEAR);

    xc->pitchbend += XXIH.fei.flg & XXM_ENV_FLT ? 0 : frq_envelope;

    /* From Takashi Iwai's awedrv FAQ:
     *
     * Q3.9: Many clicking noises can be heard in some midi files.
     *    A: If this happens when panning status changes, it is due to the
     *       restriction of Emu8000 chip. Try -P option with drvmidi. This
     *       option suppress the realtime pan position change. Otherwise,
     *       it may be a bug.
     */

    finalpan = xc->pan + (pan_envelope - 32) * (128 - abs (xc->pan - 128)) / 32;
    finalpan = xc->masterpan + (finalpan - 128) *
			(128 - abs (xc->masterpan - 128)) / 128;

    if (XXIH.fei.flg & XXM_ENV_FLT) {
	cutoff = xc->filter.cutoff * frq_envelope / 0xff;
    } else {
	cutoff = 0xff;
    }

    /* Do tremor */
    if (xc->tcnt_up || xc->tcnt_dn) {
	if (xc->tcnt_up > 0) {
	    if (xc->tcnt_up--)
		xc->tcnt_dn = LSN(xc->tremor);
	} else {
	    finalvol = 0;
	    if (xc->tcnt_dn--)
		xc->tcnt_up = MSN(xc->tremor);
	}
    }

    /* Do keyoff */
    if (xc->keyoff) {
	if (!--xc->keyoff)
	    SET(RELEASE);
    }

    /* Volume slides happen in all frames but the first, except when the
     * "volume slide on all frames" flag is set.
     */
    if (t % p->tempo || HAS_QUIRK(QUIRK_VSALL)) {
	if (!chn && p->gvol_flag) {
	    m->volume += p->gvol_slide;
	    if (m->volume < 0)
		m->volume = 0;
	    else if (m->volume > p->gvol_base)
		m->volume = p->gvol_base;
	}
	if (TEST(VOL_SLIDE) || TEST_PER(VOL_SLIDE))
	    xc->volume += xc->v_val;

	if (TEST_PER(VOL_SLIDE)) {
	    if (xc->v_val > 0 && xc->volume > m->volbase) {
		xc->volume = m->volbase;
		RESET_PER(VOL_SLIDE);
	    }
	    if (xc->v_val < 0 && xc->volume < 0) {
		xc->volume = 0;
		RESET_PER(VOL_SLIDE);
	    }
	}

	if (TEST(VOL_SLIDE_2))
	    xc->volume += xc->v_val2;

	if (TEST(TRK_VSLIDE))
	    xc->mastervol += xc->trk_val;
    }

    /* "Fine" sliding effects are processed in the first frame of each row,
     * and standard slides in the rest of the frames.
     */
    if (t % p->tempo || HAS_QUIRK(QUIRK_PBALL)) {
	/* Do pan and pitch sliding */
	if (TEST(PAN_SLIDE)) {
	    xc->pan += xc->p_val;
	    if (xc->pan < 0)
		xc->pan = 0;
	    else if (xc->pan > 0xff)
		xc->pan = 0xff;
	}

	if (TEST(PITCHBEND) || TEST_PER(PITCHBEND))
	    xc->period += xc->f_val;

	/* Do tone portamento */
	if (TEST(TONEPORTA) || TEST_PER(TONEPORTA)) {
	    xc->period += xc->s_sgn * xc->s_val;
	    if ((xc->s_sgn * xc->s_end) < (xc->s_sgn * xc->period)) {
		xc->period = xc->s_end;
		RESET(TONEPORTA);
		RESET_PER(TONEPORTA);
   	    }
	}

	/* Workaround for panic.s3m (from Toru Egashira's NSPmod) */
	if (xc->period <= 8)
	    xc->volume = 0;
    }

    if (t % p->tempo == 0) {
	/* Process "fine" effects */
	if (TEST(FINE_VOLS))
	    xc->volume += xc->v_fval;
	if (TEST(FINE_BEND))
	    xc->period = (4 * xc->period + xc->f_fval) / 4;
	if (TEST(TRK_FVSLIDE))
	    xc->mastervol += xc->trk_fval;
	if (TEST(FINE_NSLIDE)) {
	    xc->note += xc->ns_fval;
	    xc->period = note_to_period(xc->note, xc->finetune,
				m->mod.flg & XXM_FLG_LINEAR);
	}
    }

    if (xc->volume < 0)
	xc->volume = 0;
    else if (xc->volume > p->gvol_base)
	xc->volume = p->gvol_base;

    if (xc->mastervol < 0)
	xc->mastervol = 0;
    else if (xc->mastervol > p->gvol_base)
	xc->mastervol = p->gvol_base;

    if (m->mod.flg & XXM_FLG_LINEAR) {
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

    /* Update vibrato, tremolo and arpeggio indexes */
    update_instrument_vibrato(m, xc);
    update_lfo(&xc->vibrato);
    update_lfo(&xc->tremolo);
    update_stepper(&xc->arpeggio);

    /* Process MED synth arpeggio */
    med_arp = get_med_arp(m, xc);

    /* Adjust pitch and pan, then play the note */
    finalpan = o->outfmt & XMP_FMT_MONO ?
	0 : (finalpan - 0x80) * o->mix / 100;
    xmp_drv_setbend(ctx, chn, xc->pitchbend + get_stepper(&xc->arpeggio) + med_arp);
    xmp_drv_setpan(ctx, chn, m->flags & XMP_CTL_REVERSE ? -finalpan : finalpan);
    xmp_drv_setvol(ctx, chn, finalvol);

    if (cutoff < 0xff && (m->flags & XMP_CTL_FILTER)) {
	filter_setup(ctx, xc, cutoff);
	xmp_drv_seteffect(ctx, chn, XMP_FX_FILTER_B0, xc->filter.B0);
	xmp_drv_seteffect(ctx, chn, XMP_FX_FILTER_B1, xc->filter.B1);
	xmp_drv_seteffect(ctx, chn, XMP_FX_FILTER_B2, xc->filter.B2);
    } else {
	cutoff = 0xff;
    }

    xmp_drv_seteffect(ctx, chn, XMP_FX_RESONANCE, xc->filter.resonance);
    xmp_drv_seteffect(ctx, chn, XMP_FX_CUTOFF, cutoff);
}

int xmp_player_start(xmp_context opaque)
{
	struct xmp_context *ctx = (struct xmp_context *)opaque;
	struct xmp_player_context *p = &ctx->p;
	struct xmp_driver_context *d = &ctx->d;
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_options *o = &ctx->o;
	struct flow_control *f = &p->flow;
	int ret;

	xmp_smix_on(ctx);

	p->gvol_slide = 0;
	p->gvol_base = m->volbase;
	p->pos = f->ord = o->start;
	p->frame = 0;
	p->row = 0;
	p->time = 0;

	if (m->mod.len == 0 || m->mod.chn == 0) {
		/* set variables to sane state */
		m->flags &= ~XMP_CTL_LOOP;
		f->ord = p->scan_ord = 0;
		p->row = p->scan_row = 0;
		f->end_point = 0;
		return 0;
	}

	f->num_rows = m->mod.xxp[m->mod.xxo[f->ord]]->rows;

	/* Skip invalid patterns at start (the seventh laboratory.it) */
	while (f->ord < m->mod.len && m->mod.xxo[f->ord] >= m->mod.pat)
		f->ord++;

	m->volume = m->xxo_info[f->ord].gvl;
	p->bpm = m->xxo_info[f->ord].bpm;
	p->tempo = m->xxo_info[f->ord].tempo;

	p->tick_time = m->rrate / p->bpm;
	f->jumpline = m->xxo_info[f->ord].start_row;
	f->playing_time = 0;
	f->end_point = p->scan_num;

	if ((ret = xmp_drv_on(ctx, m->mod.chn)) != 0)
		return ret;

	smix_resetvar(ctx);

	f->jump = -1;

	p->fetch_ctl = calloc(m->mod.chn, sizeof (int));
	if (p->fetch_ctl == NULL)
		goto err;

	f->loop_stack = calloc(d->numchn, sizeof (int));
	if (f->loop_stack == NULL)
		goto err1;

	f->loop_start = calloc(d->numchn, sizeof (int));
	if (f->loop_start == NULL)
		goto err2;

	p->xc_data = calloc(d->numchn, sizeof (struct channel_data));
	if (p->xc_data == NULL)
		goto err3;

	if (m->synth->init(ctx, o->freq) < 0)
		goto err4;

	m->synth->reset(ctx);

	reset_channel(ctx);

	return 0;

err4:
	free(p->xc_data);
err3:
	free(f->loop_start);
err2:
	free(f->loop_stack);
err1:
	free(p->fetch_ctl);
err:
	return -1;
}

int xmp_player_frame(xmp_context opaque)
{
	struct xmp_context *ctx = (struct xmp_context *)opaque;
	struct xmp_player_context *p = &ctx->p;
	struct xmp_driver_context *d = &ctx->d;
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_options *o = &ctx->o;
	struct flow_control *f = &p->flow;
	int i;

	if (p->pause) {
		return 0;
	}

	/* check reposition */
	if (f->ord != p->pos) {
		if (p->pos == -1)
			p->pos++;		/* restart module */

		if (p->pos == -2) {		/* set by xmp_module_stop */
			return -1;		/* that's all folks */
		}

		if (p->pos == 0)
			f->end_point = p->scan_num;

		f->ord = p->pos;
		if (m->xxo_info[f->ord].tempo)
			p->tempo = m->xxo_info[f->ord].tempo;
		p->bpm = m->xxo_info[f->ord].bpm;
		p->tick_time = m->rrate / p->bpm;
		m->volume = m->xxo_info[f->ord].gvl;
		f->jump = f->ord;
		p->time = (double)m->xxo_info[f->ord].time / 1000;
		f->jumpline = m->xxo_info[f->ord].start_row;
		p->row = -1;
		f->pbreak = 1;
		f->ord--;
		xmp_drv_reset(ctx);
		reset_channel(ctx);
		goto next_row;
	}

	/* check new row */

	if (p->frame == 0) {			/* first frame in row */
		/* check end of module */
	    	if ((~m->flags & XMP_CTL_LOOP) && f->ord == p->scan_ord &&
					p->row == p->scan_row) {
			if (!f->end_point--)
				return -1;
		}

		p->gvol_flag = 0;
		if (f->skip_fetch) {
			f->skip_fetch = 0;
		} else {
			read_row(ctx, m->mod.xxo[f->ord], p->row);
		}
	}

	/* play_frame */
	for (i = 0; i < d->numchn; i++)
		play_channel(ctx, i, p->frame);

	if (o->time && (o->time < f->playing_time))	/* expired time */
		return -1;

	if (HAS_QUIRK(QUIRK_MEDBPM)) {
		f->playing_time += m->rrate * 33 / (100 * p->bpm * 125);
		p->time += m->rrate * 33 / (100 * p->bpm * 125);
	} else {
		f->playing_time += m->rrate / (100 * p->bpm);
		p->time += m->rrate / (100 * p->bpm);
	}

	p->frame++;

	if (p->frame >= (p->tempo * (1 + f->delay))) {
next_row:
		/* If break during pattern delay, next row is skipped.
		 * See corruption.mod order 1D (pattern 0D) last line:
		 * EE2 + D31 ignores D00 in order 1C line 31
		 * Reported by The Welder <welder@majesty.net>, Jan 14 2012
		 */
		if (f->delay && f->pbreak) {
			f->skip_fetch = 1;
		}

		p->frame = 0;
		f->delay = 0;

		if (f->pbreak) {
			f->pbreak = 0;

			if (f->jump != -1) {
				f->ord = f->jump - 1;
				f->jump = -1;
				goto next_order;
			}

			goto next_order;
		}

		if (f->loop_chn) {
			p->row = f->loop_start[--f->loop_chn] - 1;
			f->loop_chn = 0;
		}

		p->row++;

		/* check end of pattern */
		if (p->row >= f->num_rows) {
next_order:
    			f->ord++;

			/* Restart module */
			if (f->ord >= m->mod.len) {
    				f->ord = ((uint32)m->mod.rst > m->mod.len ||
					(uint32)m->mod.xxo[m->mod.rst] >=
					m->mod.pat) ?  0 : m->mod.rst;
				m->volume = m->xxo_info[f->ord].gvl;
			}

			/* Skip invalid patterns */
			if (m->mod.xxo[f->ord] >= m->mod.pat) {
    				f->ord++;
    				goto next_order;
			}

			p->time = (double)m->xxo_info[f->ord].time / 1000;

			f->num_rows = m->mod.xxp[m->mod.xxo[f->ord]]->rows;
			if (f->jumpline >= f->num_rows)
				f->jumpline = 0;
			p->row = f->jumpline;
			f->jumpline = 0;

			p->pos = f->ord;

			/* Reset persistent effects at new pattern */
			if (HAS_QUIRK(QUIRK_PERPAT)) {
				int chn;
				for (chn = 0; chn < m->mod.chn; chn++)
					p->xc_data[chn].per_flags = 0;
			}
		}
	}

	xmp_smix_softmixer(ctx);

#ifndef ANDROID
	if (o->dump == 1) {
		int i;

		for (i = 0; i < m->mod.chn; i++) {
			struct channel_data *c = &p->xc_data[i];

			printf("%d %d %d %d %d %d %lf %d %d %d %d %lf "
			       "%d %d %d %d %d %d %d %d %d %d\n",
					f->ord,
					m->mod.xxo[f->ord],
					p->row,
					p->frame,
					p->bpm,
					p->tempo,
					p->tick_time,
					i,
					c->note,
					c->key,
					c->volume,
					c->period,

					c->pitchbend,
					c->finetune,
					c->ins,
					c->smp,
					c->insdef,
					c->pan,
					c->masterpan,
					c->mastervol,
					c->delay,
					c->rcount
			);
		}
	}
#endif

	return 0;
}
    

void xmp_player_end(xmp_context opaque)
{
	struct xmp_context *ctx = (struct xmp_context *)opaque;
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &ctx->m;
	struct flow_control *f = &p->flow;

	xmp_drv_off(ctx);
	m->synth->deinit(ctx);

	if (m->mod.len == 0 || m->mod.chn == 0)
                return;

	free(p->xc_data);
	free(f->loop_start);
	free(f->loop_stack);
	free(p->fetch_ctl);

	xmp_smix_off(ctx);
}


void xmp_player_get_info(xmp_context opaque, struct xmp_module_info *info)
{
	struct xmp_context *ctx = (struct xmp_context *)opaque;
	struct xmp_player_context *p = &ctx->p;
	struct xmp_smixer_context *s = &ctx->s;
	struct xmp_mod_context *m = &ctx->m;
	int chn, i;

	chn = m->mod.chn;

	info->mod = &m->mod;
	info->order = p->pos;
	info->pattern = m->mod.xxo[p->pos];
	info->row = p->row;
	info->frame = p->frame;
	info->tempo = p->tempo;
	info->bpm = p->bpm;
	info->buffer = s->buffer;
	info->size = s->ticksize * s->mode * s->resol;

	for (i = 0; i < chn; i++) {
		struct channel_data *c = &p->xc_data[i];

		info->channel_info[i].period = c->period * 1000000;
		info->channel_info[i].note = c->key;
		info->channel_info[i].instrument = c->ins;
		info->channel_info[i].sample = c->smp;
		info->channel_info[i].volume = c->volume;
		info->channel_info[i].pan = c->pan;
	}

	info->mod = &m->mod;
}
