/* Extended Module Player
 * Copyright (C) 1997-2010 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "driver.h"
#include "mixer.h"
#include "synth.h"
#include "smix.h"
#include "period.h"
#include "convert.h"


#define FLAG_ITPT	0x01
#define FLAG_16_BITS	0x02
#define FLAG_STEREO	0x04
#define FLAG_FILTER	0x08
#define FLAG_REVLOOP	0x10
#define FLAG_ACTIVE	0x20
#define FLAG_SYNTH	0x40
#define FIDX_FLAGMASK	(FLAG_ITPT | FLAG_16_BITS | FLAG_STEREO | FLAG_FILTER)

#define DOWNMIX_SHIFT	 12
#define LIM8_HI		 127
#define LIM8_LO		-127
#define LIM12_HI	 4095
#define LIM12_LO	-4096
#define LIM16_HI	 32767
#define LIM16_LO	-32768


void    smix_mn8norm      (struct voice_info *, int *, int, int, int, int);
void    smix_mn8itpt      (struct voice_info *, int *, int, int, int, int);
void    smix_mn16norm     (struct voice_info *, int *, int, int, int, int);
void    smix_mn16itpt     (struct voice_info *, int *, int, int, int, int);
void    smix_st8norm      (struct voice_info *, int *, int, int, int, int);
void    smix_st8itpt      (struct voice_info *, int *, int, int, int, int);
void    smix_st16norm     (struct voice_info *, int *, int, int, int, int);
void    smix_st16itpt	  (struct voice_info *, int *, int, int, int, int);

void    smix_mn8itpt_flt  (struct voice_info *, int *, int, int, int, int);
void    smix_mn16itpt_flt (struct voice_info *, int *, int, int, int, int);
void    smix_st8itpt_flt  (struct voice_info *, int *, int, int, int, int);
void    smix_st16itpt_flt (struct voice_info *, int *, int, int, int, int);


/* Array index:
 *
 * bit 0: 0=non-interpolated, 1=interpolated
 * bit 1: 0=8 bit, 1=16 bit
 * bit 2: 0=mono, 1=stereo
 */

static void (*mix_fn[])() = {
    /* unfiltered */
    smix_mn8norm,
    smix_mn8itpt,
    smix_mn16norm,
    smix_mn16itpt,
    smix_st8norm,
    smix_st8itpt,
    smix_st16norm,
    smix_st16itpt,

    /* filtered */
    smix_mn8norm,
    smix_mn8itpt_flt,
    smix_mn16norm,
    smix_mn16itpt_flt,
    smix_st8norm,
    smix_st8itpt_flt,
    smix_st16norm,
    smix_st16itpt_flt
};


/* Downmix 32bit samples to 8bit, signed or unsigned, mono or stereo output */
static void out_su8norm(char *dest, int *src, int num, int amp, int flags)
{
	int smp;
	int shift = DOWNMIX_SHIFT + 8 - amp;

	for (; num--; src++, dest++) {
		smp = *src >> shift;
		if (smp > LIM8_HI) {
			*dest = LIM8_HI;
		} else if (smp < LIM8_LO) {
			*dest = LIM8_LO;
		} else {
			*dest = smp;
		}
	}
}


/* Downmix 32bit samples to 16bit, signed or unsigned, mono or stereo output */
static void out_su16norm(int16 * dest, int *src, int num, int amp, int flags)
{
	int smp;
	int shift = DOWNMIX_SHIFT - amp;

	for (; num--; src++, dest++) {
		smp = *src >> shift;
		if (smp > LIM16_HI) {
			*dest = LIM16_HI;
		} else if (smp < LIM16_LO) {
			*dest = LIM16_LO;
		} else {
			*dest = smp;
		}
	}
}


/* Prepare the mixer for the next tick */
void smix_resetvar(struct xmp_context *ctx)
{
    struct xmp_player_context *p = &ctx->p;
    struct xmp_mod_context *m = &ctx->m;
    struct xmp_smixer_context *s = &ctx->s;
    struct xmp_options *o = &ctx->o;

    s->ticksize = m->quirk & QUIRK_MEDBPM ?
	o->freq * m->rrate * 33 / p->bpm / 12500 :
    	o->freq * m->rrate / p->bpm / 100;

    s->dtright = s->dtleft = 0;
    memset(s->buf32b, 0, s->ticksize * s->mode * sizeof (int));
}


/* Hipolito's rampdown anticlick */
static void smix_rampdown(struct xmp_context *ctx, int voc, int32 *buf, int cnt)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_smixer_context *s = &ctx->s;
    int smp_l, smp_r;
    int dec_l, dec_r;

    if (voc < 0) {
	/* initialize */
	smp_r = s->dtright;
	smp_l = s->dtleft;
    } else {
	smp_r = d->voice_array[voc].sright;
	smp_l = d->voice_array[voc].sleft;
	d->voice_array[voc].sright = d->voice_array[voc].sleft = 0;
    }

    if (!smp_l && !smp_r)
	return;

    if (!buf) {
	buf = s->buf32b;
	cnt = SLOW_RELEASE; //s->ticksize;
    }
    if (!cnt)
	return;

    dec_r = smp_r / cnt;
    dec_l = smp_l / cnt;

    while ((smp_r || smp_l) && cnt--) {
	if (dec_r > 0)
	    *(buf++) += smp_r > dec_r ? (smp_r -= dec_r) : (smp_r = 0);
	else
	    *(buf++) += smp_r < dec_r ? (smp_r -= dec_r) : (smp_r = 0);

	if (dec_l > 0)
	    *(buf++) += smp_l > dec_l ? (smp_l -= dec_l) : (smp_l = 0);
	else
	    *(buf++) += smp_l < dec_l ? (smp_l -= dec_l) : (smp_l = 0);
    }
}


/* Ok, it's messy, but it works :-) Hipolito */
static void smix_anticlick(struct xmp_context *ctx, int voc, int vol, int pan, int *buf, int cnt)
{
    int oldvol, newvol, pan0;
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_smixer_context *s = &ctx->s;
    struct voice_info *vi = &d->voice_array[voc];

    /* From: Mirko Buffoni <mirbuf@gmail.com>
     * To: Claudio Matsuoka <cmatsuoka@gmail.com>
     * Date: Nov 29, 2007 6:45 AM
     *	
     * Put PAN SEPARATION to 100. Then it crashes. Other modules crash when
     * PAN SEPARATION = 100, (...) moving separation one step behind, stop
     * crashes.
     */
    pan0 = vi->pan;
    if (pan0 < -127)
	pan0 = -127;

    if (vi->vol) {
	oldvol = vi->vol * (0x80 - pan0);
	newvol = vol * (0x80 - pan);
	vi->sright -= vi->sright / oldvol * newvol;

	oldvol = vi->vol * (0x80 + pan0);
	newvol = vol * (0x80 + pan);
	vi->sleft -= vi->sleft / oldvol * newvol;
    }

    if (!buf) {
	s->dtright += vi->sright;
	s->dtleft += vi->sleft;
	vi->sright = vi->sleft = 0;
    } else {
	smix_rampdown(ctx, voc, buf, cnt);
    }
}


/* Fill the output buffer calling one of the handlers. The buffer contains
 * sound for one tick (a PAL frame or 1/50s for standard vblank-timed mods)
 */
void xmp_smix_softmixer(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_smixer_context *s = &ctx->s;
    struct xmp_mod_context *m = &ctx->m;
    struct xmp_options *o = &ctx->o;
    struct xmp_sample *xxs;
    struct voice_info *vi;
    int samples, tick, lps, lpe;
    int vol_l, vol_r, step, voc;
    int prv_l, prv_r;
    int synth = 1;
    int *buf_pos;
    int size;

    smix_rampdown(ctx, -1, NULL, 0);	/* Anti-click */

    for (voc = d->maxvoc; voc--; ) {
	vi = &d->voice_array[voc];

	if (vi->chn < 0)
	    continue;

	if (vi->period < 1) {
	    xmp_drv_resetvoice(ctx, voc, 1);
	    continue;
	}

	buf_pos = s->buf32b;
	vol_r = vi->vol * (0x80 - vi->pan);
	vol_l = vi->vol * (0x80 + vi->pan);

	if (vi->fidx & FLAG_SYNTH) {
	    if (synth) {
    		m->synth->mixer(ctx, buf_pos, s->ticksize, vol_l >> 7,
					vol_r >> 7, vi->fidx & FLAG_STEREO);
	        synth = 0;
	    }
	    continue;
	}

	step = ((int64)s->pbase << SMIX_SHIFT) / vi->period;

	if (step == 0)	/* otherwise m5v-nwlf.t crashes */
	    continue;

	xxs = &m->mod.xxs[vi->smp];

	/* This is for bidirectional sample loops */
	if (vi->fidx & FLAG_REVLOOP)
	    step = -step;

	/* Sample loop processing. Offsets in samples, not bytes */
	lps = xxs->lps;
	lpe = xxs->lpe;

	/* check for Protracker loop */
	if (xxs->flg & XMP_SAMPLE_LOOP_FULL && xxs->flg & XMP_SAMPLE_LOOP_FIRST) {
	    lpe = xxs->len - 1;
	}

	for (tick = s->ticksize; tick; ) {
	    /* How many samples we can write before the loop break or
	     * sample end... */
	    samples = 1 + (((int64)(vi->end - vi->pos) << SMIX_SHIFT)
		- vi->frac) / step;

	    if (step > 0) {
		if (vi->end < vi->pos)
		    samples = 0;
	    } else {
		if (vi->end > vi->pos)
		    samples = 0;
	    }

	    /* Ok, this shouldn't happen, but in 'Languede.mod'... */
	    if (samples < 0)
		samples = 0;
	    
	    /* ...inside the tick boundaries */
	    if (samples > tick)
		samples = tick;

	    if (vi->vol) {
		int idx;
		int mix_size = s->mode * samples;
		int mixer = vi->fidx & FIDX_FLAGMASK;

		/* Hipolito's anticlick routine */
		idx = mix_size;
		if (idx < 2)
		    idx = 2;
		prv_r = buf_pos[idx - 2];
		prv_l = buf_pos[idx - 1];

		/* "Beautiful Ones" apparently uses 0xfe as 'no filter' :\ */
		if (vi->filter.cutoff >= 0xfe)
		    mixer &= ~FLAG_FILTER;

		/* Call the output handler */
		mix_fn[mixer](vi, buf_pos, samples, vol_l, vol_r, step);
		buf_pos += s->mode * samples;

		/* Hipolito's anticlick routine */
		idx = 0;
		if (mix_size < 2)
		    idx = 2;
		vi->sright = buf_pos[idx - 2] - prv_r;
		vi->sleft = buf_pos[idx - 1] - prv_l;
	    }

	    vi->frac += step * samples;
	    vi->pos += vi->frac >> SMIX_SHIFT;
	    vi->frac &= SMIX_MASK;

	    /* No more samples in this tick */
	    if (!(tick -= samples))
		continue;

	    vi->fidx ^= vi->fxor;

	    /* First sample loop run */
            if (vi->fidx == 0 || lps >= lpe) {
		smix_anticlick(ctx, voc, 0, 0, buf_pos, tick);
		xmp_drv_resetvoice(ctx, voc, 0);
		tick = 0;
		continue;
	    }

	    if ((~vi->fidx & FLAG_REVLOOP) && vi->fxor == 0) {
		vi->pos -= lpe - lps;			/* forward loop */
		if (xxs->flg & XMP_SAMPLE_LOOP_FULL) {
	            vi->end = lpe = xxs->lpe;
	            xxs->flg &= ~XMP_SAMPLE_LOOP_FIRST;
		}
	    } else {
		step = -step;			/* invert dir */
		vi->frac += step;
		/* keep bidir loop at the same size of forward loop */
		vi->pos += (vi->frac >> SMIX_SHIFT) + 1;
		vi->frac &= SMIX_MASK;
		vi->end = step > 0 ? lpe : lps;
	    }
	}
    }

    /* Render final frame */

    size = s->mode * s->ticksize;
    assert(size <= OUT_MAXLEN);

    if (o->resol > 8) {
	out_su16norm((int16 *)s->buffer, s->buf32b, size, o->amplify, o->outfmt);
    } else {
	out_su8norm(s->buffer, s->buf32b, size, o->amplify, o->outfmt);
    }

    smix_resetvar(ctx);
}


void smix_voicepos(struct xmp_context *ctx, int voc, int pos, int frac)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &ctx->m;
    struct voice_info *vi = &d->voice_array[voc];
    struct xmp_sample *xxs = &m->mod.xxs[vi->smp];
    int lpe;

    if (xxs->flg & XMP_SAMPLE_SYNTH)
	return;

    lpe = xxs->len - 1;
    if (xxs->flg & XMP_SAMPLE_LOOP && ~xxs->flg & XMP_SAMPLE_LOOP_FIRST)
	lpe = lpe > xxs->lpe ? xxs->lpe : lpe;

    if (pos >= lpe)			/* Happens often in MED synth */
	pos = 0;

    vi->pos = pos;
    vi->frac = frac;
    vi->end = lpe;

    if (vi->fidx & FLAG_REVLOOP)
	vi->fidx ^= vi->fxor;
}


void smix_setpatch(struct xmp_context *ctx, int voc, int smp)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &ctx->m;
    struct xmp_options *o = &ctx->o;
    struct voice_info *vi = &d->voice_array[voc];
    struct xmp_sample *xxs = &m->mod.xxs[smp];

    vi->smp = smp;
    vi->vol = 0;
    vi->pan = 0;
    
    if (~o->outfmt & XMP_FMT_MONO) {
	vi->fidx |= FLAG_STEREO;
    }

    if (xxs->flg & XMP_SAMPLE_SYNTH) {
	vi->fidx = FLAG_SYNTH;
	m->synth->setpatch(ctx, voc, xxs->data);
	return;
    }
    
    xmp_smix_setvol(ctx, voc, 0);

    vi->sptr = xxs->data;
    vi->fidx = m->flags & XMP_CTL_ITPT ? FLAG_ITPT | FLAG_ACTIVE : FLAG_ACTIVE;

    if (~o->outfmt & XMP_FMT_MONO) {
	vi->fidx |= FLAG_STEREO;
    }

    if (xxs->flg & XMP_SAMPLE_16BIT)
	vi->fidx |= FLAG_16_BITS;

    if (m->flags & XMP_CTL_FILTER)
	vi->fidx |= FLAG_FILTER;

    if (xxs->flg & XMP_SAMPLE_LOOP)
	vi->fxor = xxs->flg & XMP_SAMPLE_LOOP_BIDIR ? FLAG_REVLOOP : 0;
    else
	vi->fxor = vi->fidx;

    if (xxs->flg & XMP_SAMPLE_LOOP_FULL)
	xxs->flg |= XMP_SAMPLE_LOOP_FIRST;

    smix_voicepos(ctx, voc, 0, 0);
}


void smix_setnote(struct xmp_context *ctx, int voc, int note)
{
    struct xmp_driver_context *d = &ctx->d;
    struct voice_info *vi = &d->voice_array[voc];

    vi->period = note_to_period_mix(vi->note = note, 0);
    vi->attack = SLOW_ATTACK;
}


void smix_setbend(struct xmp_context *ctx, int voc, int bend)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &ctx->m;
    struct voice_info *vi = &d->voice_array[voc];

    vi->period = note_to_period_mix(vi->note, bend);

    if (vi->fidx & FLAG_SYNTH)
	m->synth->setnote(ctx, voc, vi->note, bend);
}


void xmp_smix_setvol(struct xmp_context *ctx, int voc, int vol)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &ctx->m;
    struct voice_info *vi = &d->voice_array[voc];
 
    smix_anticlick(ctx, voc, vol, vi->pan, NULL, 0);
    vi->vol = vol;

    if (vi->fidx & FLAG_SYNTH)
	m->synth->setvol(ctx, voc, vol >> 4);
}


void xmp_smix_seteffect(struct xmp_context *ctx, int voc, int type, int val)
{
    struct xmp_driver_context *d = &ctx->d;
    struct xmp_mod_context *m = &ctx->m;
    struct voice_info *vi = &d->voice_array[voc];
 
    switch (type) {
    case XMP_FX_CUTOFF:
        vi->filter.cutoff = val;
	break;
    case XMP_FX_RESONANCE:
        vi->filter.resonance = val;
	break;
    case XMP_FX_FILTER_B0:
        vi->filter.B0 = val;
	break;
    case XMP_FX_FILTER_B1:
        vi->filter.B1 = val;
	break;
    case XMP_FX_FILTER_B2:
        vi->filter.B2 = val;
	break;
    }

    if (vi->fidx & FLAG_SYNTH)
	m->synth->seteffect(ctx, voc, type, val);
}


void xmp_smix_setpan(struct xmp_context *ctx, int voc, int pan)
{
    struct xmp_driver_context *d = &ctx->d;
    struct voice_info *vi = &d->voice_array[voc];
 
    vi->pan = pan;
}


int xmp_smix_numvoices(struct xmp_context *ctx, int num)
{
    struct xmp_smixer_context *s = &ctx->s;

    if (num > s->numvoc || num < 0) {
	return s->numvoc;
    } else {
	return num;
    }
}

int xmp_smix_on(struct xmp_context *ctx)
{
	struct xmp_smixer_context *s = &ctx->s;
	struct xmp_options *o = &ctx->o;

	s->buffer = calloc(SMIX_RESMAX, OUT_MAXLEN);
	if (s->buffer == NULL)
		goto err;

	s->buf32b = calloc(sizeof (int), OUT_MAXLEN);
	if (s->buf32b == NULL)
		goto err1;

	s->numvoc = SMIX_NUMVOC;
	s->mode = o->outfmt & XMP_FMT_MONO ? 1 : 2;
	s->resol = o->resol > 8 ? 2 : 1;

	return 0;

err1:
	free(s->buffer);
err:
	return -1;
}


void xmp_smix_off(struct xmp_context *ctx)
{
    struct xmp_smixer_context *s = &ctx->s;

    free(s->buffer);
    free(s->buf32b);
    s->buf32b = NULL;
    s->buffer = NULL;
}
