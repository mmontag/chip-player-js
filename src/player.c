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
#include "virtual.h"
#include "period.h"
#include "effects.h"
#include "player.h"
#include "synth.h"
#include "mixer.h"

/* Values for multi-retrig */
static const struct retrig_control rval[] = {
    {   0,  1,  1 }, {  -1,  1,  1 }, {  -2,  1,  1 }, {  -4,  1,  1 },
    {  -8,  1,  1 }, { -16,  1,  1 }, {   0,  2,  3 }, {   0,  1,  2 },
    {   0,  1,  1 }, {   1,  1,  1 }, {   2,  1,  1 }, {   4,  1,  1 },
    {   8,  1,  1 }, {  16,  1,  1 }, {   0,  3,  2 }, {   0,  2,  1 },
    {   0,  0,  1 }	/* Note cut */
};

#define WAVEFORM_SIZE 64

/* Vibrato/tremolo waveform tables */
static const int waveform[4][WAVEFORM_SIZE] = {
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

static const struct xmp_event empty_event = { 0, 0, 0, 0, 0, 0, 0 };

/*
 * "Anyway I think this is the most brilliant piece of crap we
 *  have managed to put up!"
 *			  -- Ice of FC about "Mental Surgery"
 */

static int read_event (struct context_data *, struct xmp_event *, int, int);
static void play_channel (struct context_data *, int, int);


/* Instrument vibrato */

static inline int get_instrument_vibrato(struct module_data *m,
					 struct channel_data *xc)
{
	int mapped, type, depth, phase, sweep;
	struct xmp_instrument *instrument;
	struct xmp_subinstrument *sub;

	instrument = &m->mod.xxi[xc->ins];

	mapped = instrument->map[xc->key].ins;
	if (mapped == 0xff)
		return 0;

	sub = &instrument->sub[mapped];
	type = sub->vwf;
	depth = sub->vde;
	phase = xc->instrument_vibrato.phase;
	sweep = xc->instrument_vibrato.sweep;

	return waveform[type][phase] * depth / (1024 * (1 + sweep));
}

static inline void update_instrument_vibrato(struct module_data *m,
					     struct channel_data *xc)
{
	int mapped, rate;
	struct xmp_instrument *instrument;
	struct xmp_subinstrument *sub;

	instrument = &m->mod.xxi[xc->ins];

	mapped = instrument->map[xc->key].ins;
	if (mapped == 0xff)
		return;

	sub = &instrument->sub[mapped];
	rate = sub->vra;

	xc->instrument_vibrato.phase += rate >> 2;
	xc->instrument_vibrato.phase %= WAVEFORM_SIZE;

	if (xc->instrument_vibrato.sweep > 1)
		xc->instrument_vibrato.sweep -= 2;
	else
		xc->instrument_vibrato.sweep = 0;
}


static inline void copy_channel(struct player_data *p, int to, int from)
{
	if (to > 0 && to != from) {
		memcpy(&p->xc_data[to], &p->xc_data[from],
					sizeof (struct channel_data));
	}
}


static inline void reset_channel(struct context_data *ctx)
{
    struct player_data *p = &ctx->p;
    struct module_data *m = &ctx->m;
    struct xmp_module *mod = &m->mod;
    struct channel_data *xc;
    int i;

    m->synth->reset(ctx);
    memset(p->xc_data, 0, sizeof (struct channel_data) * p->virt.virt_channels);

    for (i = p->virt.virt_channels; i--; ) {
	xc = &p->xc_data[i];
	xc->insdef = xc->ins = xc->key = -1;
    }
    for (i = p->virt.num_tracks; i--; ) {
	xc = &p->xc_data[i];
	xc->masterpan = mod->xxc[i].pan;
	xc->mastervol = mod->xxc[i].vol;
	xc->filter.cutoff = 0xff;
    }
}


static int read_event(struct context_data *ctx, struct xmp_event *e, int chn, int ctl)
{
    struct player_data *p = &ctx->p;
    struct module_data *m = &ctx->m;
    struct xmp_module *mod = &m->mod;
    int xins, ins, note, key, flg;
    struct channel_data *xc;
    int cont_sample;
    int mapped;
    struct xmp_instrument *instrument;
    struct xmp_subinstrument *sub;

    xc = &p->xc_data[chn];

    /* Tempo affects delay and must be computed first */
    if ((e->fxt == FX_TEMPO && e->fxp < 0x20) || e->fxt == FX_S3M_TEMPO) {
	if (e->fxp) {
	    p->tempo = e->fxp;
	}
    }
    if ((e->f2t == FX_TEMPO && e->f2p < 0x20) || e->f2t == FX_S3M_TEMPO) {
	if (e->f2p) {
	    p->tempo = e->f2p;
	}
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
    ins = note = -1;
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

	if ((uint32)ins < mod->ins && mod->xxi[ins].nsm) {	/* valid ins */
	    if (!key && HAS_QUIRK(QUIRK_INSPRI)) {
		if (xins == ins)
		    flg = NEW_INS | RESET_VOL;
		else 
		    key = xc->key + 1;
	    }
	    xins = ins;
	} else {					/* invalid ins */
	    if (!HAS_QUIRK(QUIRK_NCWINS))
		virtch_resetchannel(ctx, chn);

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
	    virtch_resetchannel(ctx, chn);
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

    if ((uint32)ins < mod->ins && mod->xxi[ins].nsm)
	flg |= IS_VALID;

    if ((uint32)key < XMP_KEY_OFF && key > 0) {
	key--;

	if (key < XMP_MAX_KEYS) {
	    xc->key = key;
	} else {
	    flg &= ~IS_VALID;
	}

	if (flg & IS_VALID) {
	    if (mod->xxi[ins].map[key].ins != 0xff) {
		int mapped = mod->xxi[ins].map[key].ins;
		int transp = mod->xxi[ins].map[key].xpo;
		int smp;

		note = key + mod->xxi[ins].sub[mapped].xpo + transp;
		smp = mod->xxi[ins].sub[mapped].sid;
		if (mod->xxs[smp].len == 0) {
			smp = -1;
		}

		if (smp >= 0 && smp < mod->smp) {
	            int mapped = mod->xxi[ins].map[key].ins;
	            int to = virtch_setpatch(ctx, chn, ins, smp, note,
			mod->xxi[ins].sub[mapped].nna,
			mod->xxi[ins].sub[mapped].dct,
			mod->xxi[ins].sub[mapped].dca, ctl, cont_sample);

	            if (to < 0)
		        return -1;

	            copy_channel(p, to, chn);

	            xc->smp = smp;
		}
	    } else {
		flg &= ~(RESET_VOL | RESET_ENV | NEW_INS | NEW_NOTE);
	    }
	} else {
	    if (!HAS_QUIRK(QUIRK_CUTNWI))
		virtch_resetchannel(ctx, chn);
	}
    }


    /* Reset flags */
    xc->delay = xc->retrig.delay = 0;
    xc->flags = flg | (xc->flags & 0xff000000);	/* keep persistent flags */

    reset_stepper(&xc->arpeggio);
    xc->tremor.val = 0;

    if ((uint32)xins >= mod->ins || !mod->xxi[xins].nsm) {
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

    instrument = &m->mod.xxi[xc->ins];
    mapped = instrument->map[xc->key].ins;
    if (mapped == 0xff) {
	return 0;
    }
    sub = &instrument->sub[mapped];

    if (note >= 0) {
	xc->note = note;

	if (cont_sample == 0) {
	    virtch_voicepos(ctx, chn, xc->offset_val);
	    if (TEST(OFFSET) && HAS_QUIRK(QUIRK_FX9BUG))
		xc->offset_val <<= 1;
	}
	RESET(OFFSET);

	/* Fixed by Frederic Bujon <lvdl@bigfoot.com> */
	if (!TEST(NEW_PAN))
	    xc->pan = sub->pan;

	if (!TEST(FINETUNE))
	    xc->finetune = sub->fin;

	xc->freq.s_end = xc->period = note_to_period(note, xc->finetune,
					HAS_QUIRK(QUIRK_LINEAR));

	xc->gvl = sub->gvl;
	xc->instrument_vibrato.sweep = sub->vsw;
	xc->instrument_vibrato.phase = 0;

	xc->v_idx = xc->p_idx = xc->f_idx = 0;
	xc->filter.cutoff = sub->ifc & 0x80 ? (sub->ifc - 0x80) * 2 : 0xff;
	xc->filter.resonance = sub->ifr & 0x80 ? (sub->ifr - 0x80) * 2 : 0;

	set_lfo_phase(&xc->vibrato, 0);
	set_lfo_phase(&xc->tremolo, 0);
    }

    if (xc->key < 0) {
	return 0;
    }

    if (TEST(RESET_ENV)) {
	/* xc->fadeout = 0x8000; -- moved to fetch */
	RESET(RELEASE | FADEOUT);

    }

    if (TEST(RESET_VOL)) {
	xc->volume = sub->vol;
	SET(NEW_VOL);
    }

    if (HAS_QUIRK(QUIRK_ST3GVOL) && TEST(NEW_VOL)) {
	xc->volume = xc->volume * p->gvol.volume / m->volbase;
    }

    return 0;
}


static inline void read_row(struct context_data *ctx, int pat, int row)
{
    int count, chn;
    struct module_data *m = &ctx->m;
    struct xmp_module *mod = &m->mod;
    struct xmp_event *event;
    int control[XMP_MAX_CHANNELS];

    count = 0;
    for (chn = 0; chn < mod->chn; chn++) {
        control[chn] = 0;

	if (row < mod->xxt[TRACK_NUM(pat, chn)]->rows) {
	    event = &EVENT(pat, chn, row);
	} else {
	    event = (struct xmp_event *)&empty_event;
	}

	if (read_event(ctx, event, chn, 1) != 0) {
	    control[chn]++;
	    count++;
	}
    }

    for (chn = 0; count > 0; chn++) {
	if (control[chn]) {
	    if (row < mod->xxt[TRACK_NUM(pat, chn)]->rows) {
	        event = &EVENT(pat, chn, row);
	    } else {
	        event = (struct xmp_event *)&empty_event;
	    }
	    read_event(ctx, &EVENT(pat, chn, row), chn, 0);
	    count--;
	}
    }
}


static void play_channel(struct context_data *ctx, int chn, int t)
{
    int finalvol, finalpan, cutoff, act;
    int pan_envelope, frq_envelope;
    int med_arp, vibrato, med_vibrato;
    uint16 vol_envelope;
    struct player_data *p = &ctx->p;
    struct module_data *m = &ctx->m;
    struct mixer_data *s = &ctx->s;
    struct channel_data *xc = &p->xc_data[chn];
    struct xmp_instrument *instrument;
    int linear_bend;

    /* Do delay */
    if (xc->delay && !--xc->delay) {
	if (read_event(ctx, xc->delayed_event, chn, 1) != 0)
	    read_event(ctx, xc->delayed_event, chn, 0);
    }

    instrument = &m->mod.xxi[xc->ins];

    act = virtch_cstat(ctx, chn);
    if (act == VIRTCH_INVALID)
	return;

    if (!t && act != VIRTCH_ACTIVE) {
	if (!TEST(IS_VALID) || act == VIRTCH_ACTION_CUT) {
	    virtch_resetchannel(ctx, chn);
	    return;
	}
	xc->delay = xc->retrig.delay = 0;
	reset_stepper(&xc->arpeggio);
	xc->flags &= (0xff000000 | IS_VALID);	/* keep persistent flags */
    }

    if (!TEST(IS_VALID))
	return;

    /* Process MED synth instruments */
    xmp_med_synth(ctx, chn, xc, !t && TEST(NEW_INS | NEW_NOTE));

    if (TEST(RELEASE) && !(instrument->aei.flg & XMP_ENVELOPE_ON))
	xc->fadeout = 0;
 
    if (TEST(FADEOUT | RELEASE) || act == VIRTCH_ACTION_FADE || act == VIRTCH_ACTION_OFF) {
        if (xc->fadeout > instrument->rls) {
	    xc->fadeout -= instrument->rls;
        } else {
            xc->fadeout = 0;
        }

	if (xc->fadeout == 0) {

	    /* Setting the volume to 0 instead of resetting the channel
	     * will make us spend more CPU, but allows portamento after
	     * keyoff to continue the sample instead of resetting it.
	     * This is used in Decibelter - Cosmic 'Wegian Mamas.xm
	     * Only reset the channel in virtual channel mode so we
	     * can release it.
	     */
	     if (HAS_QUIRK(QUIRK_VIRTUAL)) {
		 virtch_resetchannel(ctx, chn);
		 return;
	     } else {
		 xc->volume = 0;
	     }
	}
    }

    vol_envelope = get_envelope(&instrument->aei, xc->v_idx, 64);
    pan_envelope = get_envelope(&instrument->pei, xc->p_idx, 32);
    frq_envelope = get_envelope(&instrument->fei, xc->f_idx, 0);

    /* Update envelopes */
    xc->v_idx = update_envelope(&instrument->aei, xc->v_idx, DOENV_RELEASE);
    xc->p_idx = update_envelope(&instrument->pei, xc->p_idx, DOENV_RELEASE);
    xc->f_idx = update_envelope(&instrument->fei, xc->f_idx, DOENV_RELEASE);

    switch (check_envelope_fade(&instrument->aei, xc->v_idx)) {
    case -1:
	virtch_resetchannel(ctx, chn);
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
	xc->noteslide.count--;
	if (xc->noteslide.count == 0) {
	    xc->note += xc->noteslide.slide;
	    xc->period = note_to_period(xc->note, xc->finetune,
					HAS_QUIRK(QUIRK_LINEAR));
	    xc->noteslide.count = xc->noteslide.speed;
	}
    }

    /* Do cut/retrig */
    if (xc->retrig.delay) {
	if (!--xc->retrig.count) {
	    if (xc->retrig.type < 0x10)
		virtch_voicepos(ctx, chn, 0);	/* don't retrig on cut */
	    xc->volume += rval[xc->retrig.type].s;
	    xc->volume *= rval[xc->retrig.type].m;
	    xc->volume /= rval[xc->retrig.type].d;
	    xc->retrig.count = xc->retrig.delay;
	}
    }

    finalvol = xc->volume;

    if (TEST(TREMOLO))
	finalvol += get_lfo(&xc->tremolo) / 512;
    if (finalvol > m->volbase)
	finalvol = m->volbase;
    if (finalvol < 0)
	finalvol = 0;

    finalvol = (finalvol * xc->fadeout) >> 5;	/* 16 bit output */

    finalvol = (uint32)(vol_envelope *
	(HAS_QUIRK(QUIRK_ST3GVOL) ? 0x40 : p->gvol.volume) *
	xc->mastervol / 0x40 * ((int)finalvol * 0x40 / m->volbase)) >> 18;

    /* Volume translation table (for PTM, ARCH, COCO) */
    if (m->vol_table) {
	finalvol = m->volbase == 0xff ?
		m->vol_table[finalvol >> 2] << 2 :
		m->vol_table[finalvol >> 4] << 4;
    }

    if (HAS_QUIRK(QUIRK_INSVOL))
	finalvol = (finalvol * instrument->vol * xc->gvl) >> 12;


    /* Vibrato */

    med_vibrato = get_med_vibrato(xc);

    vibrato = get_instrument_vibrato(m, xc);
    if (TEST(VIBRATO) || TEST_PER(VIBRATO)) {
	vibrato += get_lfo(&xc->vibrato) >> 10;
    }


    /* IT pitch envelopes are always linear, even in Amiga period mode.
     * Each unit in the envelope scale is 1/25 semitone.
     */
    linear_bend = period_to_bend(
	xc->period + vibrato + med_vibrato,
	xc->note,
	/* xc->finetune, */
	HAS_QUIRK(QUIRK_MODRNG),
	xc->gliss,
	HAS_QUIRK(QUIRK_LINEAR));

    if (~instrument->fei.flg & XMP_ENVELOPE_FLT) {
        linear_bend += frq_envelope;
    }

    /* Pan */

    finalpan = xc->pan + (pan_envelope - 32) * (128 - abs (xc->pan - 128)) / 32;
    finalpan = xc->masterpan + (finalpan - 128) *
			(128 - abs (xc->masterpan - 128)) / 128;

    if (instrument->fei.flg & XMP_ENVELOPE_FLT) {
	cutoff = xc->filter.cutoff * frq_envelope / 0xff;
    } else {
	cutoff = 0xff;
    }

    /* Do tremor */
    if (xc->tremor.count_up || xc->tremor.count_dn) {
	if (xc->tremor.count_up > 0) {
	    if (xc->tremor.count_up--)
		xc->tremor.count_dn = LSN(xc->tremor.val);
	} else {
	    finalvol = 0;
	    if (xc->tremor.count_dn--)
		xc->tremor.count_up = MSN(xc->tremor.val);
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
	if (!chn && p->gvol.flag) {
	    p->gvol.volume += p->gvol.slide;
	    if (p->gvol.volume < 0)
		p->gvol.volume = 0;
	    else if (p->gvol.volume > m->volbase)
		p->gvol.volume = m->volbase;
	}
	if (TEST(VOL_SLIDE) || TEST_PER(VOL_SLIDE))
	    xc->volume += xc->vol.slide;

	if (TEST_PER(VOL_SLIDE)) {
	    if (xc->vol.slide > 0 && xc->volume > m->volbase) {
		xc->volume = m->volbase;
		RESET_PER(VOL_SLIDE);
	    }
	    if (xc->vol.slide < 0 && xc->volume < 0) {
		xc->volume = 0;
		RESET_PER(VOL_SLIDE);
	    }
	}

	if (TEST(VOL_SLIDE_2))
	    xc->volume += xc->vol.slide2;

	if (TEST(TRK_VSLIDE))
	    xc->mastervol += xc->trackvol.slide;
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
	    xc->period += xc->freq.slide;

	/* Do tone portamento */
	if (TEST(TONEPORTA) || TEST_PER(TONEPORTA)) {
	    xc->period += xc->freq.s_sgn * xc->freq.s_val;
	    if ((xc->freq.s_sgn * xc->freq.s_end) < (xc->freq.s_sgn * xc->period)) {
		xc->period = xc->freq.s_end;
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
	    xc->volume += xc->vol.fslide;
	if (TEST(FINE_BEND))
	    xc->period = (4 * xc->period + xc->freq.fslide) / 4;
	if (TEST(TRK_FVSLIDE))
	    xc->mastervol += xc->trackvol.fslide;
	if (TEST(FINE_NSLIDE)) {
	    xc->note += xc->noteslide.fslide;
	    xc->period = note_to_period(xc->note, xc->finetune,
					HAS_QUIRK(QUIRK_LINEAR));
	}
    }

    if (xc->volume < 0)
	xc->volume = 0;
    else if (xc->volume > m->volbase)
	xc->volume = m->volbase;

    if (xc->mastervol < 0)
	xc->mastervol = 0;
    else if (xc->mastervol > m->volbase)
	xc->mastervol = m->volbase;

    if (HAS_QUIRK(QUIRK_LINEAR)) {
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
    if (s->format & XMP_FORMAT_MONO) {
	finalpan = 0;
    } else {
	finalpan = (finalpan - 0x80) * s->mix / 100;
    }

    linear_bend += get_stepper(&xc->arpeggio) + med_arp;
    xc->pitchbend = linear_bend;
    xc->final_period = note_to_period_mix(xc->note, linear_bend);

    virtch_setbend(ctx, chn, linear_bend);
    virtch_setpan(ctx, chn, finalpan);
    virtch_setvol(ctx, chn, finalvol);

    if (cutoff < 0xff && HAS_QUIRK(QUIRK_FILTER)) {
	filter_setup(ctx, xc, cutoff);
	virtch_seteffect(ctx, chn, DSP_EFFECT_FILTER_B0, xc->filter.B0);
	virtch_seteffect(ctx, chn, DSP_EFFECT_FILTER_B1, xc->filter.B1);
	virtch_seteffect(ctx, chn, DSP_EFFECT_FILTER_B2, xc->filter.B2);
    } else {
	cutoff = 0xff;
    }

    virtch_seteffect(ctx, chn, DSP_EFFECT_RESONANCE, xc->filter.resonance);
    virtch_seteffect(ctx, chn, DSP_EFFECT_CUTOFF, cutoff);

    update_invloop(m, xc);
}

static void inject_event(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int chn;
	
	for (chn = 0; chn < mod->chn; chn++) {
		struct xmp_event *e = &p->inject_event[chn];
		if (e->flag > 0) {
			if (read_event(ctx, e, chn, 1) != 0) {
				read_event(ctx, e, chn, 0);
			}
			e->flag = 0;
		}
	}
}

static void next_order(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct flow_control *f = &p->flow;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;

	do {
    		p->ord++;

		/* Restart module */
		if (p->ord >= mod->len || mod->xxo[p->ord] == 0xff) {
    			p->ord = ((uint32)mod->rst > mod->len ||
				(uint32)mod->xxo[mod->rst] >=
				mod->pat) ?  0 : mod->rst;
			p->gvol.volume = m->xxo_info[p->ord].gvl;
		}
	} while (mod->xxo[p->ord] >= mod->pat);

	p->time = m->xxo_info[p->ord].time;

	f->num_rows = mod->xxp[mod->xxo[p->ord]]->rows;
	if (f->jumpline >= f->num_rows)
		f->jumpline = 0;
	p->row = f->jumpline;
	f->jumpline = 0;

	p->pos = p->ord;
	p->frame = 0;

	/* Reset persistent effects at new pattern */
	if (HAS_QUIRK(QUIRK_PERPAT)) {
		int chn;
		for (chn = 0; chn < mod->chn; chn++)
			p->xc_data[chn].per_flags = 0;
	}
}

static void next_row(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct flow_control *f = &p->flow;

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
			p->ord = f->jump - 1;
			f->jump = -1;
		}

		next_order(ctx);
	} else {
		if (f->loop_chn) {
			p->row = f->loop[f->loop_chn - 1].start - 1;
			f->loop_chn = 0;
		}
	
		p->row++;
	
		/* check end of pattern */
		if (p->row >= f->num_rows) {
			next_order(ctx);
		}
	}
}


int xmp_player_start(xmp_context opaque, int start, int rate, int format)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct mixer_data *s = &ctx->s;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int ret;

	mixer_on(ctx);

	p->gvol.slide = 0;
	p->gvol.volume = m->volbase;
	p->pos = p->ord = p->start = start;
	p->frame = -1;
	p->row = 0;
	p->time = 0;
	p->loop_count = 0;
	s->freq = rate;
	s->format = format;
	s->amplify = DEFAULT_AMPLIFY;
	s->mix = DEFAULT_MIX;
	s->pbase = SMIX_C4NOTE * m->c4rate / s->freq;

	/* Sanity check */
	if (mod->tpo == 0) {
		mod->tpo = 6;
	}
	if (mod->bpm == 0) {
		mod->bpm = 125;
	}

	if (mod->len == 0 || mod->chn == 0) {
		/* set variables to sane state */
		p->ord = p->scan.ord = 0;
		p->row = p->scan.row = 0;
		f->end_point = 0;
		f->num_rows = 0;
	} else {
		f->num_rows = mod->xxp[mod->xxo[p->ord]]->rows;
	}

	/* Skip invalid patterns at start (the seventh laboratory.it) */
	while (p->ord < mod->len && mod->xxo[p->ord] >= mod->pat)
		p->ord++;

	p->gvol.volume = m->xxo_info[p->ord].gvl;
	p->bpm = m->xxo_info[p->ord].bpm;
	p->tempo = m->xxo_info[p->ord].tempo;
	p->frame_time = m->time_factor * m->rrate / p->bpm;
	f->end_point = p->scan.num;

	if ((ret = virtch_on(ctx, mod->chn)) != 0)
		return ret;

	f->jumpline = 0;
	f->jump = -1;
	f->pbreak = 0;

	f->loop = calloc(p->virt.virt_channels, sizeof(struct pattern_loop));
	if (f->loop == NULL)
		goto err;

	p->xc_data = calloc(p->virt.virt_channels, sizeof(struct channel_data));
	if (p->xc_data == NULL)
		goto err1;

	if (m->synth->init(ctx, s->freq) < 0)
		goto err2;

	m->synth->reset(ctx);

	reset_channel(ctx);

	return 0;

    err2:
	free(p->xc_data);
    err1:
	free(f->loop);
    err:
	return -1;
}

int xmp_player_frame(xmp_context opaque)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int i;

	if (mod->len <= 0)
		return -1;

	/* check reposition */
	if (p->ord != p->pos) {
		if (p->pos == -2) {		/* set by xmp_module_stop */
			return -1;		/* that's all folks */
		}

		if (p->pos == -1) {
			p->pos++;		/* restart module */
		}

		if (p->pos == 0) {
			f->end_point = p->scan.num;
		}

		if (p->pos > p->scan.ord) {
			f->end_point = 0;
		}

		f->jumpline = 0;
		f->jump = -1;

		p->ord = p->pos - 1;
		next_order(ctx);

		if (m->xxo_info[p->ord].tempo)
			p->tempo = m->xxo_info[p->ord].tempo;
		p->bpm = m->xxo_info[p->ord].bpm;
		p->gvol.volume = m->xxo_info[p->ord].gvl;
		p->time = m->xxo_info[p->ord].time;

		virtch_reset(ctx);
		reset_channel(ctx);
	} else {
		p->frame++;
		if (p->frame >= (p->tempo * (1 + f->delay))) {
			next_row(ctx);
		}
	}

	/* check new row */

	if (p->frame == 0) {			/* first frame in row */
		/* check end of module */
	    	if (p->ord == p->scan.ord && p->row == p->scan.row) {
			if (f->end_point == 0) {
				p->loop_count++;
				f->end_point = p->scan.num;
				/* return -1; */
			}
			f->end_point--;
		}

		p->gvol.flag = 0;
		if (f->skip_fetch) {
			f->skip_fetch = 0;
		} else {
			read_row(ctx, mod->xxo[p->ord], p->row);
		}
	}

	inject_event(ctx);

	/* play_frame */
	for (i = 0; i < p->virt.virt_channels; i++) {
		play_channel(ctx, i, p->frame);
	}

	p->frame_time = m->time_factor * m->rrate / p->bpm;
	p->time += p->frame_time;

	mixer_softmixer(ctx);

	return 0;
}
    

void xmp_player_end(xmp_context opaque)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct flow_control *f = &p->flow;

	virtch_off(ctx);
	m->synth->deinit(ctx);

	free(p->xc_data);
	free(f->loop);

	p->xc_data = NULL;
	f->loop = NULL;

	mixer_off(ctx);
}


void xmp_player_get_info(xmp_context opaque, struct xmp_module_info *info)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct mixer_data *s = &ctx->s;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int chn, i;

	chn = m->mod.chn;

	info->mod = &m->mod;
	info->order = p->pos;
	info->pattern = mod->xxo[p->pos];
	info->row = p->row;

	if (info->pattern < mod->pat) {
		info->num_rows = mod->xxp[info->pattern]->rows;
	} else {
		info->num_rows = 0;
	}

	info->frame = p->frame;
	info->tempo = p->tempo;
	info->bpm = p->bpm;
	info->total_time = m->time;
	info->frame_time = p->frame_time * 1000;
	info->time = p->time;
	info->buffer = s->buffer;

	info->total_size = OUT_MAXLEN;
	info->buffer_size = s->ticksize;
	if (~s->format & XMP_FORMAT_MONO) {
		info->buffer_size *= 2;
	}
	if (~s->format & XMP_FORMAT_8BIT) {
		info->buffer_size *= 2;
	}

	info->volume = p->gvol.volume;
	info->loop_count = p->loop_count;
	info->virt_channels = p->virt.virt_channels;
	info->virt_used = p->virt.virt_used;

	info->mod = mod;

	if (p->xc_data != NULL) {
		for (i = 0; i < chn; i++) {
			struct channel_data *c = &p->xc_data[i];
			struct xmp_channel_info *ci = &info->channel_info[i];
			int track;
			struct xmp_event *event;
	
			ci->note = c->key;
			ci->pitchbend = c->pitchbend;
			ci->period = c->final_period;
			ci->instrument = c->ins;
			ci->sample = c->smp;
			ci->volume = c->volume;
			ci->pan = c->pan;
	
			if (info->pattern < mod->pat) {
				track = mod->xxp[info->pattern]->index[i];
				event = &mod->xxt[track]->event[info->row];
				memcpy(&ci->event, event, sizeof(*event));
			} else {
				memset(&ci->event, 0, sizeof(*event));
			}
		}
	}
}
