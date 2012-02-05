/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See docs/COPYING
 * for more information.
 */

#include <stdlib.h>
#include <string.h>
#include "virtual.h"
#include "convert.h"
#include "mixer.h"
#include "smix.h"

#define	FREE	-1
#define MAX_VOICES_CHANNEL 16


void virtch_resetvoice(struct xmp_context *ctx, int voc, int mute)
{
    struct xmp_driver_context *d = &ctx->d;
    struct voice_info *vi = &d->voice_array[voc];

    if ((uint32)voc >= d->maxvoc)
	return;

    if (mute)
	xmp_smix_setvol(ctx, voc, 0);

    d->curvoc--;
    d->ch2vo_count[vi->root]--;
    d->ch2vo_array[vi->chn] = FREE;
    memset(vi, 0, sizeof (struct voice_info));
    vi->chn = vi->root = FREE;
}


/* virtch_on (number of tracks) */
int virtch_on(struct xmp_context *ctx, int num)
{
	struct xmp_driver_context *d = &ctx->d;
	struct xmp_mod_context *m = &ctx->m;
	int i;

	d->numtrk = num;
	num = xmp_smix_numvoices(ctx, -1);

	d->numchn = d->numtrk;
	d->chnvoc = m->flags & XMP_CTL_VIRTUAL ? MAX_VOICES_CHANNEL : 1;

	if (d->chnvoc > 1)
		d->numchn += num;
	else if (num > d->numchn)
		num = d->numchn;

	num = d->maxvoc = xmp_smix_numvoices(ctx, num);

	d->voice_array = calloc(d->maxvoc, sizeof(struct voice_info));
	if (d->voice_array == NULL)
		goto err;

	d->ch2vo_array = calloc(d->numchn, sizeof(int));
	if (d->ch2vo_array == NULL)
		goto err1;

	d->ch2vo_count = calloc(d->numchn, sizeof(int));
	if (d->ch2vo_count == NULL)
		goto err2;

	for (i = 0; i < d->maxvoc; i++) {
		d->voice_array[i].chn = FREE;
		d->voice_array[i].root = FREE;
	}

	for (i = 0; i < d->numchn; i++) {
		 d->ch2vo_array[i] = FREE;
	}

	d->curvoc = d->agevoc = 0;

	return 0;

err2:
	free(d->ch2vo_array);
err1:
	free(d->voice_array);
err:
	return -1;
}


void virtch_off(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;

    if (d->numchn < 1)
	return;

    d->curvoc = d->maxvoc = 0;
    d->numchn = 0;
    d->numtrk = 0;
    free(d->voice_array);
    free(d->ch2vo_array);
    free(d->ch2vo_count);
}


void virtch_reset(struct xmp_context *ctx)
{
    struct xmp_driver_context *d = &ctx->d;
    int i;

    if (d->numchn < 1)
	return;

    xmp_smix_numvoices(ctx, d->maxvoc);

    memset(d->ch2vo_count, 0, d->numchn * sizeof (int));
    memset(d->voice_array, 0, d->maxvoc * sizeof (struct voice_info));

    for (i = 0; i < d->maxvoc; i++)
	 d->voice_array[i].chn = d->voice_array[i].root = FREE;

    for (i = 0; i < d->numchn; i++)
	d->ch2vo_array[i] = FREE;

    d->curvoc = d->agevoc = 0;
}


void virtch_resetchannel(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    xmp_smix_setvol(ctx, voc, 0);

    d->curvoc--;
    d->ch2vo_count[d->voice_array[voc].root]--;
    d->ch2vo_array[chn] = FREE;
    memset(&d->voice_array[voc], 0, sizeof (struct voice_info));
    d->voice_array[voc].chn = d->voice_array[voc].root = FREE;
}


static int drv_allocvoice(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int i, num;
    uint32 age;

    if (d->ch2vo_count[chn] < d->chnvoc) {

	/* Find free voice */
	for (i = 0; i < d->maxvoc; i++) {
	    if (d->voice_array[i].chn == FREE)
		break;
	}

	/* not found */
	if (i == d->maxvoc)
	    return -1;

	d->voice_array[i].age = d->agevoc;
	d->ch2vo_count[chn]++;
	d->curvoc++;

	return i;
    }

    /* Find oldest voice */
    num = age = FREE;
    for (i = 0; i < d->maxvoc; i++) {
	if (d->voice_array[i].root == chn && d->voice_array[i].age < age) {
	    num = i;
	    age = d->voice_array[num].age;
	}
    }

    /* Free oldest voice */
    d->ch2vo_array[d->voice_array[num].chn] = FREE;
    d->voice_array[num].age = d->agevoc;

    return num;
}


void virtch_mute(struct xmp_context *ctx, int chn, int status)
{
    struct xmp_driver_context *d = &ctx->d;

    if ((uint32)chn >= XMP_MAX_CHANNELS)
	return;

    if (status < 0)
	d->cmute_array[chn] = !d->cmute_array[chn];
    else
	d->cmute_array[chn] = status;
}


void virtch_setvol(struct xmp_context *ctx, int chn, int vol)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    if (d->voice_array[voc].root < XMP_MAX_CHANNELS && d->cmute_array[d->voice_array[voc].root])
	vol = 0;

    xmp_smix_setvol(ctx, voc, vol);

    if (!(vol || chn < d->numtrk))
	virtch_resetvoice(ctx, voc, 1);
}


void virtch_setpan(struct xmp_context *ctx, int chn, int pan)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    xmp_smix_setpan(ctx, voc, pan);
}


void virtch_seteffect(struct xmp_context *ctx, int chn, int type, int val)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    xmp_smix_seteffect(ctx, voc, type, val);
}


void virtch_setsmp(struct xmp_context *ctx, int chn, int smp)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;
    struct voice_info *vi;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    vi = &d->voice_array[voc];
    if (vi->smp == smp)
	return;

    smix_setpatch(ctx, voc, smp);
    smix_voicepos(ctx, voc, vi->pos, vi->frac);
}


int virtch_setpatch(struct xmp_context *ctx, int chn, int ins, int smp, int note, int nna, int dct, int dca, int flg, int cont_sample)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc, vfree;

    if ((uint32)chn >= d->numchn)
	return -1;

    if (ins < 0)
	smp = -1;

    if (dct) {
	for (voc = d->maxvoc; voc--;) {
	    if (d->voice_array[voc].root == chn && d->voice_array[voc].ins == ins) {
		if ((dct == XXM_DCT_INST) ||
		    (dct == XXM_DCT_SMP && d->voice_array[voc].smp == smp) ||
		    (dct == XXM_DCT_NOTE && d->voice_array[voc].note == note)) {
		    if (dca) {
			if (voc != d->ch2vo_array[chn] || d->voice_array[voc].act)
			    d->voice_array[voc].act = dca;
		    } else {
			virtch_resetvoice(ctx, voc, 1);
		    }
		}
	    }
	}
    }

    voc = d->ch2vo_array[chn];

    if (voc > FREE) {
	if (d->voice_array[voc].act && d->chnvoc > 1) {
	    if ((vfree = drv_allocvoice(ctx, chn)) > FREE) {
		d->voice_array[vfree].root = chn;
		d->voice_array[d->ch2vo_array[chn] = vfree].chn = chn;
		for (chn = d->numtrk; d->ch2vo_array[chn++] > FREE;);
		d->voice_array[voc].chn = --chn;
		d->ch2vo_array[chn] = voc;
		voc = vfree;
	    } else if (flg) {
		return -1;
	    }
	}
    } else {
	if ((voc = drv_allocvoice(ctx, chn)) < 0)
	    return -1;
	d->voice_array[d->ch2vo_array[chn] = voc].chn = chn;
	d->voice_array[voc].root = chn;
    }

    if (smp < 0) {
	virtch_resetvoice(ctx, voc, 1);
	return chn;	/* was -1 */
    }

    if (!cont_sample)
	smix_setpatch(ctx, voc, smp);

    smix_setnote(ctx, voc, note);
    d->voice_array[voc].ins = ins;
    d->voice_array[voc].act = nna;
    d->agevoc++;

    return chn;
}


void virtch_setnna(struct xmp_context *ctx, int chn, int nna)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    d->voice_array[voc].act = nna;
}


void virtch_setbend(struct xmp_context *ctx, int chn, int bend)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    smix_setbend(ctx, voc, bend);
}


void virtch_voicepos(struct xmp_context *ctx, int chn, int pos)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return;

    smix_voicepos(ctx, voc, pos, 0);
}


void virtch_pastnote(struct xmp_context *ctx, int chn, int act)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    for (voc = d->maxvoc; voc--;) {
	if (d->voice_array[voc].root == chn && d->voice_array[voc].chn >= d->numtrk) {
	    if (act == XMP_ACT_CUT)
		virtch_resetvoice(ctx, voc, 1);
	    else
		d->voice_array[voc].act = act;
	}
    }
}


int virtch_cstat(struct xmp_context *ctx, int chn)
{
    struct xmp_driver_context *d = &ctx->d;
    int voc;

    voc = d->ch2vo_array[chn];

    if ((uint32)chn >= d->numchn || (uint32)voc >= d->maxvoc)
	return XMP_CHN_DUMB;

    return chn < d->numtrk ? XMP_CHN_ACTIVE : d->voice_array[voc].act;
}
