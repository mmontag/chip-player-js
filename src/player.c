/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
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
	{   0,  0,  1 }		/* Note cut */
	
};

static const struct xmp_event empty_event = { 0, 0, 0, 0, 0, 0, 0 };

/*
 * "Anyway I think this is the most brilliant piece of crap we
 *  have managed to put up!"
 *			  -- Ice of FC about "Mental Surgery"
 */


/* From http://www.un4seen.com/forum/?topic=7554.0
 *
 * "Invert loop" effect replaces (!) sample data bytes within loop with their
 * bitwise complement (NOT). The parameter sets speed of altering the samples.
 * This effectively trashes the sample data. Because of that this effect was
 * supposed to be removed in the very next ProTracker versions, but it was
 * never removed.
 *
 * Prior to [Protracker 1.1A] this effect is called "Funk Repeat" and it moves
 * loop of the instrument (just the loop information - sample data is not
 * altered). The parameter is the speed of moving the loop.
 */

static const int invloop_table[] = {
	0, 5, 6, 7, 8, 10, 11, 13, 16, 19, 22, 26, 32, 43, 64, 128
};

static void update_invloop(struct module_data *m, struct channel_data *xc)
{
	struct xmp_sample *xxs = &m->mod.xxs[xc->smp];
	int len;

	xc->invloop.count += invloop_table[xc->invloop.speed];

	if ((xxs->flg & XMP_SAMPLE_LOOP) && xc->invloop.count >= 128) {
		xc->invloop.count = 0;
		len = xxs->lpe - xxs->lps;	

		if (HAS_QUIRK(QUIRK_FUNKIT)) {
			/* FIXME: not implemented */
		} else {
			if (++xc->invloop.pos > len) {
				xc->invloop.pos = 0;
			}

			if (~xxs->flg & XMP_SAMPLE_16BIT) {
				xxs->data[xxs->lps + xc->invloop.pos] ^= 0xff;
			}
		}
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
	memset(p->xc_data, 0,
	       sizeof(struct channel_data) * p->virt.virt_channels);

	for (i = 0; i < p->virt.virt_channels; i++) {
		xc = &p->xc_data[i];
		xc->ins = xc->key = -1;
	}
	for (i = 0; i < p->virt.num_tracks; i++) {
		xc = &p->xc_data[i];
		xc->masterpan = mod->xxc[i].pan;
		xc->mastervol = mod->xxc[i].vol;
		xc->filter.cutoff = 0xff;
	}
}

static int check_delay(struct context_data *ctx, struct xmp_event *e, int chn)
{
	struct player_data *p = &ctx->p;
	struct channel_data *xc = &p->xc_data[chn];

	/* Tempo affects delay and must be computed first */
	if ((e->fxt == FX_SPEED && e->fxp < 0x20) || e->fxt == FX_S3M_SPEED) {
		if (e->fxp) {
			p->speed = e->fxp;
		}
	}
	if ((e->f2t == FX_SPEED && e->f2p < 0x20) || e->f2t == FX_S3M_SPEED) {
		if (e->f2p) {
			p->speed = e->f2p;
		}
	}

	/* Delay the entire fetch cycle */
	if (e->fxt == FX_EXTENDED && MSN(e->fxp) == EX_DELAY) {
		xc->delay = LSN(e->fxp) + 1;
		xc->delayed_event = e;
		if (e->ins)
			xc->delayed_ins = e->ins;
		return 1;
	}
	if (e->f2t == FX_EXTENDED && MSN(e->f2p) == EX_DELAY) {
		xc->delay = LSN(e->f2p) + 1;
		xc->delayed_event = e;
		if (e->ins)
			xc->delayed_ins = e->ins;
		return 1;
	}

	return 0;
}

static inline void read_row(struct context_data *ctx, int pat, int row)
{
	int count, chn;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct player_data *p = &ctx->p;
	struct flow_control *f = &p->flow;
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

		if (check_delay(ctx, event, chn) == 0) {
			if (!f->rowdelay_set || f->rowdelay > 0) {
				if (read_event(ctx, event, chn, 1) != 0) {
					control[chn]++;
					count++;
				}
			}
		}
	}

	for (chn = 0; count > 0; chn++) {
		if (control[chn]) {
			if (row < mod->xxt[TRACK_NUM(pat, chn)]->rows) {
				event = &EVENT(pat, chn, row);
			} else {
				event = (struct xmp_event *)&empty_event;
			}
			if (check_delay(ctx, event, chn) == 0) {
				read_event(ctx, event, chn, 0);
				count--;
			}
		}
	}
}

static inline int get_channel_vol(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	int root;

	/* channel is a root channel */
	if (chn < p->virt.num_tracks)
		return p->channel_vol[chn];

	/* channel is invalid */
	if (chn >= p->virt.virt_channels)
		return 0;

	/* root is invalid */
	root = virt_getroot(ctx, chn);
	if (root < 0)
		return 0;

	return p->channel_vol[root];
}


/*
 * Update channel data
 */

static void process_volume(struct context_data *ctx, int chn, int t, int act)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct channel_data *xc = &p->xc_data[chn];
	struct xmp_instrument *instrument = &m->mod.xxi[xc->ins];
	int finalvol;
	uint16 vol_envelope;
	int gvol;

	/* Keyoff and fadeout */

	/* Keyoff event in IT doesn't reset fadeout (see jeff93.it)
	 * In XM it depends on envelope (see graff-strange_land.xm vs
	 * Decibelter - Cosmic 'Wegian Mamas.xm)
	 */
	if (!HAS_QUIRK(QUIRK_KEYOFF)) {
		if (TEST(RELEASE) && !(instrument->aei.flg & XMP_ENVELOPE_ON))
			xc->fadeout = 0;
	}

	if (TEST(FADEOUT | RELEASE) || act == VIRT_ACTION_FADE
	    || act == VIRT_ACTION_OFF) {
		if (xc->fadeout > instrument->rls) {
			xc->fadeout -= instrument->rls;
		} else {
			xc->fadeout = 0;
		}

		if (xc->fadeout == 0) {
			/* Setting volume to 0 instead of resetting the channel
			 * will use more CPU but allows portamento after keyoff
			 * to continue the sample instead of resetting it.
			 * This is used in Decibelter - Cosmic 'Wegian Mamas.xm
			 * Only reset the channel in virtual channel mode so we
			 * can release it.
			 */
			if (HAS_QUIRK(QUIRK_VIRTUAL)) {
				virt_resetchannel(ctx, chn);
				return;
			} else {
				xc->volume = 0;
			}
		}
	}

	switch (check_envelope_fade(&instrument->aei, xc->v_idx)) {
	case -1:
		virt_resetchannel(ctx, chn);
		break;
	case 0:
		break;
	default:
		if (HAS_QUIRK(QUIRK_ENVFADE)) {
			SET(FADEOUT);
		}
	}

	vol_envelope = get_envelope(&instrument->aei, xc->v_idx, 64);
	xc->v_idx = update_envelope(&instrument->aei, xc->v_idx, DOENV_RELEASE);

	finalvol = xc->volume;

	if (TEST(TREMOLO))
		finalvol += get_lfo(&xc->tremolo) / 512;

	CLAMP(finalvol, 0, m->volbase);

	finalvol = (finalvol * xc->fadeout) >> 5;	/* 16 bit output */

	if (HAS_QUIRK(QUIRK_ST3GVOL)) {
		gvol = 0x40;
	} else {
		gvol = p->gvol;
	}

	finalvol = (uint32)(vol_envelope * gvol * xc->mastervol / m->gvolbase *
				((int)finalvol * 0x40 / m->volbase)) >> 18;

	/* Apply channel volume */
	finalvol = finalvol * get_channel_vol(ctx, chn) / 100;

	/* Volume translation table (for PTM, ARCH, COCO) */
	if (m->vol_table) {
		finalvol = m->volbase == 0xff ?
		    m->vol_table[finalvol >> 2] << 2 :
		    m->vol_table[finalvol >> 4] << 4;
	}

	if (HAS_QUIRK(QUIRK_INSVOL)) {
		finalvol = (finalvol * instrument->vol * xc->gvl) >> 12;
	}

	if (xc->tremor.val) {
		if (xc->tremor.count == 0) {
			/* end of down cycle, set up counter for up  */
			xc->tremor.count = MSN(xc->tremor.val) | 0x80;
		} else if (xc->tremor.count == 0x80) {
			/* end of up cycle, set up counter for down */
			xc->tremor.count = LSN(xc->tremor.val);
		}

		xc->tremor.count--;

		if (~xc->tremor.count & 0x80) {
			finalvol = 0;
		}
	}

	xc->info_finalvol = finalvol;

	virt_setvol(ctx, chn, finalvol);
}

static void process_frequency(struct context_data *ctx, int chn, int t, int act)
{
	struct mixer_data *s = &ctx->s;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct channel_data *xc = &p->xc_data[chn];
	struct xmp_instrument *instrument = &m->mod.xxi[xc->ins];
	int linear_bend;
	int frq_envelope;
	int arp, vibrato, cutoff, resonance;

	frq_envelope = get_envelope(&instrument->fei, xc->f_idx, 0);
	xc->f_idx = update_envelope(&instrument->fei, xc->f_idx, DOENV_RELEASE);

	/* Do note slide */

	if (TEST(NOTE_SLIDE)) {
		if (xc->noteslide.count == 0) {
			xc->note += xc->noteslide.slide;
			xc->period = note_to_period(xc->note, xc->finetune,
						    HAS_QUIRK(QUIRK_LINEAR));
			xc->noteslide.count = xc->noteslide.speed;
		}
		xc->noteslide.count--;
	}

	/* Vibrato */

	vibrato = get_lfo(&xc->insvib.lfo) / (1024 * (1 + xc->insvib.sweep));

	if (TEST(VIBRATO) || TEST_PER(VIBRATO)) {
		vibrato += get_lfo(&xc->vibrato) >> 10;
	}

	linear_bend = period_to_bend(xc->period + vibrato + get_med_vibrato(xc),
				xc->note, HAS_QUIRK(QUIRK_MODRNG),
				xc->gliss, HAS_QUIRK(QUIRK_LINEAR));

	/* Envelope */

	if (~instrument->fei.flg & XMP_ENVELOPE_FLT) {
		/* IT pitch envelopes are always linear, even in Amiga period
		 * mode. Each unit in the envelope scale is 1/25 semitone.
		 */
		linear_bend += frq_envelope << 7;
	}

	/* Arpeggio */

	arp = xc->arpeggio.val[xc->arpeggio.count];
	linear_bend += (arp + get_med_arp(m, xc)) << 7;

	/* For xmp_get_frame_info() */
	xc->info_pitchbend = linear_bend >> 7;
	xc->info_period = note_to_period_mix(xc->note, linear_bend);

	virt_setbend(ctx, chn, linear_bend);

	/* Process filter */

	if (!HAS_QUIRK(QUIRK_FILTER)) {
		return;
	}

	if (instrument->fei.flg & XMP_ENVELOPE_FLT) {
		cutoff = xc->filter.cutoff * frq_envelope >> 8;
	} else {
		cutoff = xc->filter.cutoff;
	}
	resonance = xc->filter.resonance;

	if (cutoff > 0xff) {
		cutoff = 0xff;
	} else if (cutoff < 0xff) {
		int a0, b0, b1;
		filter_setup(s->freq, cutoff, resonance, &a0, &b0, &b1);
		virt_seteffect(ctx, chn, DSP_EFFECT_FILTER_A0, a0);
		virt_seteffect(ctx, chn, DSP_EFFECT_FILTER_B0, b0);
		virt_seteffect(ctx, chn, DSP_EFFECT_FILTER_B1, b1);
		virt_seteffect(ctx, chn, DSP_EFFECT_RESONANCE, resonance);
	}

	/* Always set cutoff */
	virt_seteffect(ctx, chn, DSP_EFFECT_CUTOFF, cutoff);
}

static void process_pan(struct context_data *ctx, int chn, int t, int act)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct mixer_data *s = &ctx->s;
	struct channel_data *xc = &p->xc_data[chn];
	struct xmp_instrument *instrument = &m->mod.xxi[xc->ins];
	int finalpan, panbrello = 0;
	int pan_envelope;

	pan_envelope = get_envelope(&instrument->pei, xc->p_idx, 32);
	xc->p_idx = update_envelope(&instrument->pei, xc->p_idx, DOENV_RELEASE);

	if (TEST(PANBRELLO))
		panbrello = get_lfo(&xc->panbrello) / 512;

	finalpan = xc->pan.val + panbrello + (pan_envelope - 32) *
				(128 - abs(xc->pan.val - 128)) / 32;
	finalpan = xc->masterpan + (finalpan - 128) *
				(128 - abs(xc->masterpan - 128)) / 128;

	if (s->format & XMP_FORMAT_MONO) {
		finalpan = 0;
	} else {
		finalpan = (finalpan - 0x80) * s->mix / 100;
	}

	CLAMP(finalpan, -128, 127);

	xc->info_finalpan = finalpan + 0x80;

	virt_setpan(ctx, chn, finalpan);
}

static void update_volume(struct context_data *ctx, int chn, int t)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct channel_data *xc = &p->xc_data[chn];

	/* Volume slides happen in all frames but the first, except when the
	 * "volume slide on all frames" flag is set.
	 */
	if (t % p->speed != 0 || HAS_QUIRK(QUIRK_VSALL)) {
		if (TEST(GVOL_SLIDE)) {
			p->gvol += xc->gvol.slide;
		}

		if (TEST(VOL_SLIDE) || TEST_PER(VOL_SLIDE)) {
			xc->volume += xc->vol.slide;
		}

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

		if (TEST(VOL_SLIDE_2)) {
			xc->volume += xc->vol.slide2;
		}
		if (TEST(TRK_VSLIDE)) {
			xc->mastervol += xc->trackvol.slide;
		}
	}

	if (t % p->speed == 0) {
		/* Process "fine" effects */
		if (TEST(FINE_VOLS))
			xc->volume += xc->vol.fslide;

		if (TEST(TRK_FVSLIDE))
			xc->mastervol += xc->trackvol.fslide;

		if (TEST(GVOL_SLIDE))
			p->gvol += xc->gvol.fslide;
	}

	/* Clamp volumes */
	CLAMP(xc->volume, 0, m->volbase);
	CLAMP(p->gvol, 0, m->gvolbase);
	CLAMP(xc->mastervol, 0, m->volbase);

	update_lfo(&xc->tremolo);
}

static void update_frequency(struct context_data *ctx, int chn, int t)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct channel_data *xc = &p->xc_data[chn];

	if (t % p->speed != 0 || HAS_QUIRK(QUIRK_PBALL)) {
		if (TEST(PITCHBEND) || TEST_PER(PITCHBEND))
			xc->period += xc->freq.slide;

		/* Do tone portamento */
		if (TEST(TONEPORTA) || TEST_PER(TONEPORTA)) {
			xc->period += xc->freq.s_sgn * xc->freq.s_val;
			if ((xc->freq.s_sgn * xc->freq.s_end) <
			    (xc->freq.s_sgn * xc->period)) {
				/* reached end */
				xc->period = xc->freq.s_end;
				xc->freq.s_sgn = 0;
				RESET(TONEPORTA);
				RESET_PER(TONEPORTA);
			}
		} 

		/* Check for invalid periods (from Toru Egashira's NSPmod)
	         * panic.s3m has negative periods
		 * ambio.it uses low (~8) period values
		 */
		if (xc->period < 1) {
			xc->volume = 0;
		}
	}

	if (t % p->speed == 0) {
		if (TEST(FINE_BEND)) {
			xc->period = (4 * xc->period + xc->freq.fslide) / 4;
		}
		if (TEST(FINE_NSLIDE)) {
			xc->note += xc->noteslide.fslide;
			xc->period = note_to_period(xc->note, xc->finetune,
						    HAS_QUIRK(QUIRK_LINEAR));
		}
	}

	if (HAS_QUIRK(QUIRK_LINEAR)) {
		CLAMP(xc->period, MIN_PERIOD_L, MAX_PERIOD_L);
	} else {
		CLAMP(xc->period, MIN_PERIOD_A, MAX_PERIOD_A);
	}

	xc->arpeggio.count++;
	xc->arpeggio.count %= xc->arpeggio.size;

	update_lfo(&xc->vibrato);

	/* Update instrument vibrato */

	update_lfo(&xc->insvib.lfo);
	if (xc->insvib.sweep > 1) {
		xc->insvib.sweep -= 2;
	} else {
		xc->insvib.sweep = 0;
	}
}

static void update_pan(struct context_data *ctx, int chn, int t)
{
	struct player_data *p = &ctx->p;
	struct channel_data *xc = &p->xc_data[chn];

	if (TEST(PAN_SLIDE)) {
		if (t % p->speed == 0) {
			xc->pan.val += xc->pan.fslide;
		} else {
			xc->pan.val += xc->pan.slide;
		}

		if (xc->pan.val < 0) {
			xc->pan.val = 0;
		} else if (xc->pan.val > 0xff) {
			xc->pan.val = 0xff;
		}
	}

	update_lfo(&xc->panbrello);
}

static void play_channel(struct context_data *ctx, int chn, int t)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct channel_data *xc = &p->xc_data[chn];
	int act;

	xc->info_finalvol = 0;

	/* Do delay */
	if (xc->delay > 0) {
		if (--xc->delay == 0) {
			if (read_event(ctx, xc->delayed_event, chn, 1) != 0)
				read_event(ctx, xc->delayed_event, chn, 0);
		}
	}

	act = virt_cstat(ctx, chn);
	if (act == VIRT_INVALID) {
		/* We need this to keep processing global volume slides */
		update_volume(ctx, chn, t);
		return;
	}

	if (!t && act != VIRT_ACTIVE) {
		if (!IS_VALID_INSTRUMENT(xc->ins) || act == VIRT_ACTION_CUT) {
			virt_resetchannel(ctx, chn);
			return;
		}
	}

	if (!IS_VALID_INSTRUMENT(xc->ins))
		return;

	/* Process MED synth instruments */
	med_synth(ctx, chn, xc, !t && TEST(NEW_INS | NEW_NOTE));

	/* Do cut/retrig */
	if (TEST(RETRIG)) {
		if (--xc->retrig.count <= 0) {
			if (xc->retrig.type < 0x10) {
				/* don't retrig on cut */
				virt_voicepos(ctx, chn, 0);
			}
			xc->volume += rval[xc->retrig.type].s;
			xc->volume *= rval[xc->retrig.type].m;
			xc->volume /= rval[xc->retrig.type].d;
                	xc->retrig.count = LSN(xc->retrig.val);
		}
        }

	process_volume(ctx, chn, t, act);
	process_frequency(ctx, chn, t, act);
	process_pan(ctx, chn, t, act);

	update_volume(ctx, chn, t);
	update_frequency(ctx, chn, t);
	update_pan(ctx, chn, t);

	/* Do keyoff */
	if (xc->keyoff) {
		if (!--xc->keyoff)
			SET(RELEASE);
	}

	if (HAS_QUIRK(QUIRK_INVLOOP)) {
		update_invloop(m, xc);
	}

	xc->info_position = virt_getvoicepos(ctx, chn);
}

/*
 * Event injection
 */

static void inject_event(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int chn;
	
	for (chn = 0; chn < mod->chn; chn++) {
		struct xmp_event *e = &p->inject_event[chn];
		if (e->_flag > 0) {
			if (read_event(ctx, e, chn, 1) != 0) {
				read_event(ctx, e, chn, 0);
			}
			e->_flag = 0;
		}
	}
}

/*
 * Sequencing
 */

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
			if (mod->rst > mod->len ||
			    mod->xxo[mod->rst] >= mod->pat ||
			    p->ord < m->seq_data[p->sequence].entry_point) {
				p->ord = m->seq_data[p->sequence].entry_point;
			} else {
				if (get_sequence(ctx, mod->rst) == p->sequence) {
					p->ord = mod->rst;
				} else {
					p->ord = m->seq_data[p->sequence].entry_point;
				}
			}

			p->gvol = m->xxo_info[p->ord].gvl;
		}
	} while (mod->xxo[p->ord] >= mod->pat);

	p->current_time = m->xxo_info[p->ord].time;

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
	
		if (f->rowdelay == 0) {
			p->row++;
			f->rowdelay_set = 0;
		} else {
			f->rowdelay--;
		}
	
		/* check end of pattern */
		if (p->row >= f->num_rows) {
			next_order(ctx);
		}
	}
}

/*
 * Set note action for virt_pastnote
 */
void player_set_release(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	struct channel_data *xc = &p->xc_data[chn];

	SET(RELEASE);
}

void player_set_fadeout(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	struct channel_data *xc = &p->xc_data[chn];

	SET(FADEOUT);
}


int xmp_start_player(xmp_context opaque, int rate, int format)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct mixer_data *s = &ctx->s;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int i;
	int ret = 0;

	if (mixer_on(ctx, rate, format, m->c4rate) < 0)
		return -XMP_ERROR_INTERNAL;

	p->gvol = m->volbase;
	p->pos = p->ord = 0;
	p->frame = -1;
	p->row = 0;
	p->current_time = 0;
	p->loop_count = 0;

	/* Unmute all channels and set default volume */
	for (i = 0; i < XMP_MAX_CHANNELS; i++) {
		p->channel_mute[i] = 0;
		p->channel_vol[i] = 100;
	}

	/* Skip invalid patterns at start (the seventh laboratory.it) */
	while (p->ord < mod->len && mod->xxo[p->ord] >= mod->pat) {
		p->ord++;
	}
	/* Check if all positions skipped */
	if (p->ord >= mod->len) {
		mod->len = 0;
	}

	if (mod->len == 0 || mod->chn == 0) {
		/* set variables to sane state */
		p->ord = p->scan[0].ord = 0;
		p->row = p->scan[0].row = 0;
		f->end_point = 0;
		f->num_rows = 0;
	} else {
		f->num_rows = mod->xxp[mod->xxo[p->ord]]->rows;
		f->end_point = p->scan[0].num;
	}

	p->gvol = m->xxo_info[p->ord].gvl;
	p->bpm = m->xxo_info[p->ord].bpm;
	p->speed = m->xxo_info[p->ord].speed;
	p->frame_time = m->time_factor * m->rrate / p->bpm;

	if (virt_on(ctx, mod->chn) != 0) {
		ret = -XMP_ERROR_INTERNAL;
		goto err;
	}

	f->jumpline = 0;
	f->jump = -1;
	f->pbreak = 0;
	f->rowdelay_set = 0;

	f->loop = calloc(p->virt.virt_channels, sizeof(struct pattern_loop));
	if (f->loop == NULL) {
		ret = -XMP_ERROR_SYSTEM;
		goto err;
	}

	p->xc_data = calloc(p->virt.virt_channels, sizeof(struct channel_data));
	if (p->xc_data == NULL) {
		ret = -XMP_ERROR_SYSTEM;
		goto err1;
	}

	if (m->synth->init(ctx, s->freq) < 0) {
		ret = -XMP_ERROR_INTERNAL;
		goto err2;
	}

	m->synth->reset(ctx);

	reset_channel(ctx);

	return 0;

    err2:
	free(p->xc_data);
    err1:
	free(f->loop);
    err:
	return ret;
}

int xmp_play_frame(xmp_context opaque)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int i;

	if (mod->len <= 0 || mod->xxo[p->ord] == 0xff) {
		return -XMP_END;
	}

	/* check reposition */
	if (p->ord != p->pos) {
		int start = m->seq_data[p->sequence].entry_point;

		if (p->pos == -2) {		/* set by xmp_module_stop */
			return -XMP_END;	/* that's all folks */
		}

		if (p->pos == -1) {
			/* restart sequence */
			p->pos = start;
		}

		if (p->pos == start) {
			f->end_point = p->scan[p->sequence].num;
		}

		/* Check if lands after a loop point */
		if (p->pos > p->scan[p->sequence].ord) {
			f->end_point = 0;
		}

		f->jumpline = 0;
		f->jump = -1;

		p->ord = p->pos - 1;

		/* Stay inside our subsong */
		if (p->ord < start) {
			p->ord = start - 1;
		}

		next_order(ctx);

		if (m->xxo_info[p->ord].speed)
			p->speed = m->xxo_info[p->ord].speed;
		p->bpm = m->xxo_info[p->ord].bpm;
		p->gvol = m->xxo_info[p->ord].gvl;
		p->current_time = m->xxo_info[p->ord].time;

		virt_reset(ctx);
		reset_channel(ctx);
	} else {
		p->frame++;
		if (p->frame >= (p->speed * (1 + f->delay))) {
			next_row(ctx);
		}
	}

	/* check new row */

	if (p->frame == 0) {			/* first frame in row */
		/* check end of module */
	    	if (p->ord == p->scan[p->sequence].ord &&
				p->row == p->scan[p->sequence].row) {
			if (f->end_point == 0) {
				p->loop_count++;
				f->end_point = p->scan[p->sequence].num;
				/* return -1; */
			}
			f->end_point--;
		}

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
	p->current_time += p->frame_time;

	mixer_softmixer(ctx);

	return 0;
}
    
void xmp_end_player(xmp_context opaque)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct flow_control *f = &p->flow;

	virt_off(ctx);
	m->synth->deinit(ctx);

	free(p->xc_data);
	free(f->loop);

	p->xc_data = NULL;
	f->loop = NULL;

	mixer_off(ctx);
}

void xmp_get_module_info(xmp_context opaque, struct xmp_module_info *info)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;

	memcpy(info->md5, m->md5, 16);
	info->mod = mod;
	info->comment = m->comment;
	info->num_sequences = m->num_sequences;
	info->seq_data = m->seq_data;
	info->vol_base = m->volbase;
}

void xmp_get_frame_info(xmp_context opaque, struct xmp_frame_info *info)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct mixer_data *s = &ctx->s;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int chn, i;

	chn = mod->chn;

	if (p->pos >= 0 && p->pos < mod->len) {
		info->pos = p->pos;
	} else {
		info->pos = 0;
	}

	info->pattern = mod->xxo[info->pos];

	if (info->pattern < mod->pat) {
		info->num_rows = mod->xxp[info->pattern]->rows;
	} else {
		info->num_rows = 0;
	}

	info->row = p->row;
	info->frame = p->frame;
	info->speed = p->speed;
	info->bpm = p->bpm;
	info->total_time = p->scan[p->sequence].time;
	info->frame_time = p->frame_time * 1000;
	info->time = p->current_time;
	info->buffer = s->buffer;

	info->total_size = XMP_MAX_FRAMESIZE;
	info->buffer_size = s->ticksize;
	if (~s->format & XMP_FORMAT_MONO) {
		info->buffer_size *= 2;
	}
	if (~s->format & XMP_FORMAT_8BIT) {
		info->buffer_size *= 2;
	}

	info->volume = p->gvol;
	info->loop_count = p->loop_count;
	info->virt_channels = p->virt.virt_channels;
	info->virt_used = p->virt.virt_used;

	info->sequence = p->sequence;

	if (p->xc_data != NULL) {
		for (i = 0; i < chn; i++) {
			struct channel_data *c = &p->xc_data[i];
			struct xmp_channel_info *ci = &info->channel_info[i];
			struct xmp_track *track;
			struct xmp_event *event;
			int trk;
	
			ci->note = c->key;
			ci->pitchbend = c->info_pitchbend;
			ci->period = c->info_period;
			ci->position = c->info_position;
			ci->instrument = c->ins;
			ci->sample = c->smp;
			ci->volume = c->info_finalvol >> 4;
			ci->pan = c->info_finalpan;
			ci->reserved = 0;
			memset(&ci->event, 0, sizeof(*event));
	
			if (info->pattern < mod->pat && info->row < info->num_rows) {
				trk = mod->xxp[info->pattern]->index[i];
				track = mod->xxt[trk];
				if (info->row < track->rows) {
					event = &track->event[info->row];
					memcpy(&ci->event, event, sizeof(*event));
				}
			}
		}
	}
}
